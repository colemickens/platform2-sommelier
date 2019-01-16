// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/diagnosticsd/diagnosticsd_ec_event_service.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <utility>

#include <base/bind.h>
#include <base/files/file_path.h>
#include <base/logging.h>
#include <base/memory/ref_counted.h>
#include <base/posix/eintr_wrapper.h>

namespace diagnostics {

namespace internal {

// This is the background ("monitoring") thread delegate used by
// |DiagnosticsdEcEventService|.
class EcEventMonitoringThreadDelegate final
    : public base::DelegateSimpleThread::Delegate {
 public:
  using OnEventAvailableCallback =
      base::RepeatingCallback<void(const DiagnosticsdEcEventService::EcEvent&)>;

  // |DiagnosticsdEcEventService| guarantees that the unowned pointer and file
  // descriptors outlive this delegate. This delegate will post
  // |on_event_available_callback| on the |foreground_task_runner| when an EC
  // event is available and it will post |on_shutdown_callback| on the
  // |foreground_task_runner| when it is shutting down.
  EcEventMonitoringThreadDelegate(
      int event_fd,
      int16_t event_fd_events,
      int shutdown_fd,
      scoped_refptr<base::SequencedTaskRunner> foreground_task_runner,
      OnEventAvailableCallback on_event_available_callback,
      base::OnceClosure on_shutdown_callback)
      : foreground_task_runner_(foreground_task_runner),
        on_event_available_callback_(std::move(on_event_available_callback)),
        on_shutdown_callback_(std::move(on_shutdown_callback)) {
    fds[0] = pollfd{event_fd, event_fd_events, 0};
    fds[1] = pollfd{shutdown_fd, POLLIN, 0};
  }

  ~EcEventMonitoringThreadDelegate() override = default;

  void Run() override {
    while (true) {
      int retval =
          HANDLE_EINTR(poll(fds, 2 /* nfds */, -1 /* infinite timeout */));
      if (retval < 0) {
        if (errno == EINTR) {
          // Non-critical error, we must retry.
          continue;
        }
        PLOG(ERROR)
            << "EC event poll error. Shutting down EC monitoring thread";
        break;
      }
      if (fds[1].events & fds[1].revents) {
        // Exit: the main thread requested our shutdown by writing data into
        // |shutdown_fd_|.
        break;
      }
      if ((fds[0].revents & POLLERR) || (fds[1].revents & POLLERR)) {
        LOG(ERROR) << "EC event POLLERR poll error. Shutting down EC"
                      " monitoring thread";
        break;
      }
      if ((fds[0].events & fds[0].revents) == 0) {
        // No data available for reading from |event_fd_|, so proceed to poll()
        // to wait for new events.
        continue;
      }

      DiagnosticsdEcEventService::EcEvent ec_event;
      if (HANDLE_EINTR(read(fds[0].fd, &ec_event, sizeof(ec_event))) > 0) {
        foreground_task_runner_->PostTask(
            FROM_HERE, base::BindOnce(on_event_available_callback_, ec_event));
        if (lseek(fds[0].fd, 0, SEEK_SET) == -1) {
          VPLOG(2) << "Unable to lseek event file";
        }
      }
    }

    foreground_task_runner_->PostTask(FROM_HERE,
                                      std::move(on_shutdown_callback_));
  }

 private:
  // Pollfd array, where |fds[0]| is a real sysfs fd and |fds[1]| is a fake fd
  // used to shutdown this monitoring thread delegate.
  // Not owned.
  pollfd fds[2];

  // The |SequencedTaskRunner| this object is posting tasks to. It is accessed
  // from the monitoring thread.
  scoped_refptr<base::SequencedTaskRunner> foreground_task_runner_;

  OnEventAvailableCallback on_event_available_callback_;
  base::OnceClosure on_shutdown_callback_;
};

}  // namespace internal

DiagnosticsdEcEventService::DiagnosticsdEcEventService(Delegate* delegate)
    : message_loop_(base::MessageLoop::current()), delegate_(delegate) {
  DCHECK(message_loop_);
  DCHECK(delegate_);
}

DiagnosticsdEcEventService::~DiagnosticsdEcEventService() {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  DCHECK(!monitoring_thread_);
}

bool DiagnosticsdEcEventService::Start() {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  DCHECK(!monitoring_thread_);

  auto event_file_path = root_dir_.Append(kEcEventSysfsPath);
  event_fd_.reset(HANDLE_EINTR(
      open(event_file_path.value().c_str(), O_RDONLY | O_NONBLOCK)));
  if (!event_fd_.is_valid()) {
    PLOG(ERROR) << "Unable to open sysfs event file: "
                << event_file_path.value();
    return false;
  }

  shutdown_fd_.reset(eventfd(0, EFD_NONBLOCK));
  if (!shutdown_fd_.is_valid()) {
    PLOG(ERROR) << "Unable to create eventfd";
    return false;
  }

  monitoring_thread_delegate_ =
      std::make_unique<internal::EcEventMonitoringThreadDelegate>(
          event_fd_.get(), event_fd_events_, shutdown_fd_.get(),
          message_loop_->task_runner(),
          base::BindRepeating(&DiagnosticsdEcEventService::OnEventAvailable,
                              base::Unretained(this)),
          base::BindOnce(&DiagnosticsdEcEventService::OnShutdown,
                         base::Unretained(this)));
  monitoring_thread_ = std::make_unique<base::DelegateSimpleThread>(
      monitoring_thread_delegate_.get(),
      "DiagnosticsdEcEventMonitoring" /* name_prefix */);
  monitoring_thread_->Start();
  return true;
}

void DiagnosticsdEcEventService::Shutdown(base::Closure on_shutdown_callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  DCHECK(on_shutdown_callback_.is_null());
  DCHECK(!on_shutdown_callback.is_null());

  if (!monitoring_thread_) {
    on_shutdown_callback.Run();
    return;
  }

  on_shutdown_callback_ = on_shutdown_callback;

  ShutdownMonitoringThread();
}

void DiagnosticsdEcEventService::ShutdownMonitoringThread() {
  // Due to |eventfd| documentation to invoke |poll()| on |shutdown_fd_| file
  // descriptor we must write any 8-byte value greater than 0 except
  // |0xffffffffffffffff|.
  uint64_t counter = 1;
  if (HANDLE_EINTR(write(shutdown_fd_.get(), &counter, sizeof(counter)) !=
                   sizeof(counter))) {
    PLOG(ERROR)
        << "Unable to write data in fake fd to shutdown EC event service";
  }
}

void DiagnosticsdEcEventService::OnEventAvailable(const EcEvent& ec_event) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  delegate_->SendGrpcEcEventToDiagnosticsProcessor(ec_event);
}

void DiagnosticsdEcEventService::OnShutdown() {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  monitoring_thread_->Join();
  monitoring_thread_.reset();
  monitoring_thread_delegate_.reset();

  if (!on_shutdown_callback_.is_null()) {
    on_shutdown_callback_.Run();
    on_shutdown_callback_.Reset();
  }
}

}  // namespace diagnostics
