// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_WILCO_DTC_SUPPORTD_DIAGNOSTICSD_EC_EVENT_SERVICE_H_
#define DIAGNOSTICS_WILCO_DTC_SUPPORTD_DIAGNOSTICSD_EC_EVENT_SERVICE_H_

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>

#include <base/callback.h>
#include <base/files/file_path.h>
#include <base/files/scoped_file.h>
#include <base/macros.h>
#include <base/message_loop/message_loop.h>
#include <base/sequence_checker_impl.h>
#include <base/threading/simple_thread.h>

#include "diagnostics/wilco_dtc_supportd/ec_constants.h"

namespace diagnostics {

namespace internal {
class EcEventMonitoringThreadDelegate;
}  // namespace internal

// Subscribes on EC events and redirects EC events to diagnostics
// processor.
class DiagnosticsdEcEventService final {
 public:
  struct alignas(2) EcEvent {
    EcEvent() { memset(this, 0, sizeof(EcEvent)); }

    EcEvent(uint16_t size, uint16_t type, const uint16_t data[6])
        : size(size), type(type) {
      memset(this->data, 0, sizeof(this->data));
      memcpy(this->data, data,
             std::min(sizeof(this->data), size * sizeof(data[0])));
    }

    bool operator==(const EcEvent& other) const {
      return memcmp(this, &other, sizeof(*this)) == 0;
    }

    // |size| is number of received event words from EC driver items in |data|.
    uint16_t size;
    uint16_t type;
    uint16_t data[6];
  };

  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Called when event from EC was received.
    //
    // Calls diagnostics processor |HandleEcNotification| gRPC function with
    // |payload| in request.
    virtual void SendGrpcEcEventToDiagnosticsProcessor(
        const EcEvent& ec_event) = 0;
  };

  explicit DiagnosticsdEcEventService(Delegate* delegate);
  ~DiagnosticsdEcEventService();

  // Starts service.
  bool Start();

  // Shutdowns service.
  void Shutdown(base::Closure on_shutdown_callback);

  // Overrides the file system root directory for file operations in tests.
  void set_root_dir_for_testing(const base::FilePath& root_dir) {
    root_dir_ = root_dir;
  }

  // Overrides the |event_fd_events_| in tests.
  void set_event_fd_events_for_testing(int16_t events) {
    event_fd_events_ = events;
  }

 private:
  // Signal via writing to the |shutdown_fd_| that the monitoring thread should
  // shut down. Once the monitoring thread handles this event and gets ready
  // for shutting down, it will reply by scheduling an invocation of
  // OnShutdown() on the foreground thread.
  void ShutdownMonitoringThread();

  // This is called on the |message_loop_->task_runner()| when new EC event
  // was received by background monitoring thread.
  void OnEventAvailable(const EcEvent& ec_event);

  // This is called on the |message_loop_->task_runner()| when the background
  // monitoring thread is shutting down.
  void OnShutdown();

  base::MessageLoop* const message_loop_;

  // This callback will be invoked after current service shutdown.
  base::Closure on_shutdown_callback_;

  // Unowned. The delegate should outlive this instance.
  Delegate* const delegate_;

  // The file system root directory. Can be overridden in tests.
  base::FilePath root_dir_{"/"};

  // EC event |event_fd_| and |event_fd_events_| are using for |poll()|
  // function in |monitoring_thread_|. Both can be overridden in tests.
  base::ScopedFD event_fd_;
  int16_t event_fd_events_ = kEcEventFilePollEvents;

  // Shutdown event fd. It is used to stop |poll()| immediately and shutdown
  // |monitoring_thread_|.
  base::ScopedFD shutdown_fd_;

  // The delegate which will be executed on the |monitoring_thread_|.
  std::unique_ptr<internal::EcEventMonitoringThreadDelegate>
      monitoring_thread_delegate_;
  // The backgroung thread monitoring the EC sysfs file for upcoming events.
  std::unique_ptr<base::SimpleThread> monitoring_thread_;

  base::SequenceCheckerImpl sequence_checker_;

  DISALLOW_COPY_AND_ASSIGN(DiagnosticsdEcEventService);
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_WILCO_DTC_SUPPORTD_DIAGNOSTICSD_EC_EVENT_SERVICE_H_
