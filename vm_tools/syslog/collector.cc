// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_tools/syslog/collector.h"

#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <string.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sys/un.h>

#include <linux/vm_sockets.h>  // Needs to come after sys/socket.h

#include <string>
#include <utility>

#include <base/bind.h>
#include <base/bind_helpers.h>
#include <base/callback.h>
#include <base/location.h>
#include <base/logging.h>
#include <base/memory/ptr_util.h>
#include <base/message_loop/message_loop.h>
#include <base/posix/eintr_wrapper.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_piece.h>
#include <base/strings/stringprintf.h>
#include <base/threading/thread_task_runner_handle.h>
#include <base/time/time.h>
#include <chromeos/scoped_minijail.h>
#include <grpc++/grpc++.h>

#include "vm_tools/syslog/parser.h"

using std::string;

namespace pb = google::protobuf;

namespace vm_tools {
namespace syslog {
namespace {
// Periodic interval for flushing buffered logs.
constexpr int64_t kTimerFlushMilliseconds = 5000;

// Periodic interval for flushing buffered logs during testing.
constexpr int64_t kTimerFlushMillisecondsForTesting = 500;

// Maximum size the buffer can reach before logs are immediately flushed.
constexpr size_t kBufferThreshold = 4096;

// Size of the largest syslog record as defined by RFC3164.
constexpr size_t kMaxSyslogRecord = 1024;

// Size of the largest kernel log record as defined in include/linux/printk.h.
constexpr size_t kMaxKernelRecord = 8192;

// Max number of records we should attempt to read out of the socket at a time.
constexpr int kMaxRecordCount = 11;

// Path to the standard syslog listening path.
constexpr char kDevLog[] = "/dev/log";

// Path to the dev node for kernel messages.
constexpr char kDevKmsg[] = "/dev/kmsg";

// Known host port for the LogCollector service.
constexpr unsigned int kLogCollectorPort = 9999;

// Path to the standard empty directory where we will jail the daemon.
constexpr char kVarEmpty[] = "/var/empty";

// Name for the "syslog" user and group.
constexpr char kSyslog[] = "syslog";

// Path to the file that stores the last kernel log sequence number that was
// sent to the LogCollector service.
constexpr char kKernelSequenceFile[] = "/run/syslog_kernel_sequence";

// Opens and reads the contents of the file used to keep track of the sequence
// number of the last kernel log sent to the LogCollector service.  Returns
// true and fills in |fd| and |sequence| with the file descriptor for the file
// and the sequence number respectively on success.  Returns false and leaves
// |fd| and |sequence| unchanged if the file does not exist or there was an
// error in any of the steps.
bool GetLastKernelSequence(base::ScopedFD* fd, uint64_t* sequence) {
  base::ScopedFD desc(open(kKernelSequenceFile, O_RDWR | O_CLOEXEC));
  if (!desc.is_valid()) {
    if (errno != ENOENT) {
      PLOG(WARNING) << "Failed to open " << kKernelSequenceFile;
    }
    return false;
  }

  char buf[32];
  ssize_t count = read(desc.get(), buf, sizeof(buf) - 1);
  if (count < 0) {
    PLOG(WARNING) << "Failed to read from " << kKernelSequenceFile;
    return false;
  }
  buf[count] = '\0';

  uint64_t value = 0;
  if (!base::StringToUint64(base::StringPiece(buf, count), &value)) {
    LOG(WARNING) << "Unable to convert value in " << kKernelSequenceFile
                 << " to a uint64_t";
    return false;
  }

  *fd = std::move(desc);
  *sequence = value;

  return true;
}

}  // namespace

std::unique_ptr<Collector> Collector::Create(base::Closure shutdown_closure) {
  auto collector =
      base::WrapUnique<Collector>(new Collector(std::move(shutdown_closure)));

  if (!collector->Init()) {
    collector.reset();
  }

  return collector;
}

void Collector::OnFileCanReadWithoutBlocking(int fd) {
  if (fd == signal_fd_.get()) {
    signalfd_siginfo info;
    if (read(signal_fd_.get(), &info, sizeof(info)) != sizeof(info)) {
      PLOG(ERROR) << "Failed to read from signalfd";
    }
    DCHECK_EQ(info.ssi_signo, SIGTERM);

    FlushLogs();
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, shutdown_closure_);
    return;
  }

  DCHECK(fd == syslog_fd_.get() || fd == kmsg_fd_.get());

  // The use of base::Unretained is safe here because the callback does not
  // escape this function.
  base::Callback<bool(void)> read_one;
  if (fd == syslog_fd_.get()) {
    read_one =
        base::Bind(&Collector::ReadOneSyslogRecord, base::Unretained(this));
  } else {
    read_one =
        base::Bind(&Collector::ReadOneKernelRecord, base::Unretained(this));
  }

  bool more = true;
  for (int i = 0; i < kMaxRecordCount && more; ++i) {
    more = read_one.Run();

    // Send all buffered records immediately if we've crossed the threshold.
    if (buffered_size_ > kBufferThreshold) {
      FlushLogs();
      timer_.Reset();
    }
  }
}

void Collector::OnFileCanWriteWithoutBlocking(int fd) {
  NOTREACHED();
}

Collector::Collector(base::Closure shutdown_closure)
    : syslog_controller_(FROM_HERE),
      kmsg_controller_(FROM_HERE),
      signal_controller_(FROM_HERE),
      shutdown_closure_(std::move(shutdown_closure)),
      weak_factory_(this) {}

bool Collector::Init() {
  // Start listening on the syslog socket.
  syslog_fd_.reset(socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0));

  if (!syslog_fd_.is_valid()) {
    PLOG(ERROR) << "Failed to create unix domain socket";
    return false;
  }

  // Make sure that any previous socket is cleaned up before attempting to bind
  // to it again.  We don't really care whether the unlink succeeds or not.
  HANDLE_EINTR(unlink(kDevLog));

  struct sockaddr_un sun = {
      .sun_family = AF_UNIX,
  };
  strncpy(sun.sun_path, kDevLog, sizeof(sun.sun_path));

  if (bind(syslog_fd_.get(), reinterpret_cast<struct sockaddr*>(&sun),
           sizeof(sun)) != 0) {
    PLOG(ERROR) << "Failed to bind logging socket";
    return false;
  }

  // Give everyone write permissions to the socket.
  if (chmod(sun.sun_path, 0666) != 0) {
    PLOG(ERROR) << "Unable to change permissions for syslog socket";
    return false;
  }

  bool ret = base::MessageLoopForIO::current()->WatchFileDescriptor(
      syslog_fd_.get(), true /* persistent */,
      base::MessageLoopForIO::WATCH_READ, &syslog_controller_, this);
  if (!ret) {
    LOG(ERROR) << "Failed to watch syslog file descriptor";
    return false;
  }

  // Get the sequence number of the last kernel log message that was sent to
  // LogCollector service, if any.
  if (GetLastKernelSequence(&kernel_sequence_fd_, &kernel_sequence_)) {
    LOG(INFO) << "Resuming kernel log messages at sequence number "
              << kernel_sequence_;
  } else {
    // There was no sequence file or there was an error.  Start from the first
    // kernel log message.
    if (unlink(kKernelSequenceFile) != 0 && errno != ENOENT) {
      PLOG(ERROR) << "Unable to remove old kernel log sequence file";
      return false;
    }

    kernel_sequence_fd_.reset(open(
        kKernelSequenceFile, O_WRONLY | O_CLOEXEC | O_CREAT | O_EXCL, 0600));
    if (!kernel_sequence_fd_.is_valid()) {
      PLOG(ERROR) << "Unable to create kernel log sequence file";
      return false;
    }
    kernel_sequence_ = 0;
  }

  // Start listening for kernel log messages.
  kmsg_fd_.reset(
      HANDLE_EINTR(open(kDevKmsg, O_RDONLY | O_CLOEXEC | O_NONBLOCK)));
  if (!kmsg_fd_.is_valid()) {
    PLOG(ERROR) << "Failed to open " << kDevKmsg;
    return false;
  }

  ret = base::MessageLoopForIO::current()->WatchFileDescriptor(
      kmsg_fd_.get(), true /*persistent*/, base::MessageLoopForIO::WATCH_READ,
      &kmsg_controller_, this);
  if (!ret) {
    LOG(ERROR) << "Failed to watch kmsg file descriptor";
    return false;
  }

  // Start listening for SIGTERM.
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGTERM);

  signal_fd_.reset(signalfd(-1, &mask, SFD_CLOEXEC | SFD_NONBLOCK));
  if (!signal_fd_.is_valid()) {
    PLOG(ERROR) << "Unable to create signalfd";
    return false;
  }
  ret = base::MessageLoopForIO::current()->WatchFileDescriptor(
      signal_fd_.get(), true /*persistent*/, base::MessageLoopForIO::WATCH_READ,
      &signal_controller_, this);
  if (!ret) {
    LOG(ERROR) << "Failed to watch signal file descriptor";
    return false;
  }

  // Block the standard SIGTERM handler since we will be getting it via the
  // signalfd.
  sigprocmask(SIG_BLOCK, &mask, nullptr);

  // Figure out the boot time so that we can timestamp kernel logs.
  struct sysinfo info;
  if (sysinfo(&info) != 0) {
    PLOG(ERROR) << "Failed to read sysinfo";
    return false;
  }
  boot_time_ = base::Time::Now() - base::TimeDelta::FromSeconds(info.uptime);

  // Create the stub to the LogCollector service on the host.
  stub_ = vm_tools::LogCollector::NewStub(grpc::CreateChannel(
      base::StringPrintf("vsock:%u:%u", VMADDR_CID_HOST, kLogCollectorPort),
      grpc::InsecureChannelCredentials()));
  if (!stub_) {
    LOG(ERROR) << "Failed to create stub for LogCollector service";
    return false;
  }

  // Start a timer to periodically flush logs.
  timer_.Start(FROM_HERE,
               base::TimeDelta::FromMilliseconds(kTimerFlushMilliseconds),
               base::Bind(&Collector::FlushLogs, weak_factory_.GetWeakPtr()));

  // Start a new log request buffer.
  syslog_request_ = pb::Arena::CreateMessage<vm_tools::LogRequest>(&arena_);
  kmsg_request_ = pb::Arena::CreateMessage<vm_tools::LogRequest>(&arena_);
  buffered_size_ = 0;

  // Drop all unnecessary privileges.
  ScopedMinijail jail(minijail_new());
  if (!jail) {
    PLOG(ERROR) << "Failed to create minijail";
    return false;
  }

  minijail_change_user(jail.get(), kSyslog);
  minijail_change_group(jail.get(), kSyslog);
  minijail_no_new_privs(jail.get());

  // Pivot into an empty directory where we have no permissions.
  minijail_namespace_vfs(jail.get());
  minijail_enter_pivot_root(jail.get(), kVarEmpty);

  minijail_enter(jail.get());

  // Everything succeeded.
  return true;
}

void Collector::FlushLogs() {
  if (syslog_request_->records_size() <= 0 &&
      kmsg_request_->records_size() <= 0) {
    // Nothing to do.  Just return.
    return;
  }

  if (syslog_request_->records_size() > 0) {
    grpc::ClientContext ctx;
    vm_tools::EmptyMessage response;
    grpc::Status status =
        stub_->CollectUserLogs(&ctx, *syslog_request_, &response);

    if (!status.ok()) {
      LOG(ERROR) << "Failed to send user logs to LogCollector service.  Error "
                 << "code " << status.error_code() << ": "
                 << status.error_message();
    }
  }

  if (kmsg_request_->records_size() > 0) {
    grpc::ClientContext ctx;
    vm_tools::EmptyMessage response;
    grpc::Status status =
        stub_->CollectKernelLogs(&ctx, *kmsg_request_, &response);

    if (!status.ok()) {
      LOG(ERROR) << "Failed to send kernel logs to LogCollector service.  "
                 << "Error code " << status.error_code() << ": "
                 << status.error_message();
    }
  }

  // Flush the kernel log sequence number.
  string sequence = base::Uint64ToString(kernel_sequence_);
  if (pwrite(kernel_sequence_fd_.get(), sequence.data(), sequence.size(), 0) !=
      sequence.size()) {
    PLOG(WARNING) << "Failed to update kernel log sequence number";
  }

  // Reset everything.
  arena_.Reset();
  syslog_request_ = pb::Arena::CreateMessage<vm_tools::LogRequest>(&arena_);
  kmsg_request_ = pb::Arena::CreateMessage<vm_tools::LogRequest>(&arena_);
  buffered_size_ = 0;
}

bool Collector::ReadOneSyslogRecord() {
  char buf[kMaxSyslogRecord + 1];
  ssize_t ret =
      HANDLE_EINTR(recv(syslog_fd_.get(), buf, kMaxSyslogRecord, MSG_DONTWAIT));
  if (ret < 0) {
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
      PLOG(ERROR) << "Failed to read from syslog socket";
    }
    return false;
  }

  if (ret == 0) {
    // We didn't read anything but that doesn't necessarily mean there was an
    // error.
    return true;
  }
  // Make sure the buffer is properly terminated.
  buf[ret] = '\0';

  // Attempt to parse the record.
  auto* record = pb::Arena::CreateMessage<vm_tools::LogRecord>(&arena_);
  if (!ParseSyslogRecord(buf, ret, record)) {
    LOG(ERROR) << "Failed to parse syslog record";

    // Return true here because while we failed to parse this message there may
    // still be more messages pending in the kernel buffer.
    return true;
  }

  // We have a valid entry. Update the buffered message count and store the
  // message.
  buffered_size_ += record->ByteSizeLong();

  // Safe because |record| was created by the same Arena that owns
  // |syslog_request_|.
  syslog_request_->add_records()->UnsafeArenaSwap(record);

  return true;
}

bool Collector::ReadOneKernelRecord() {
  char buf[kMaxKernelRecord];
  ssize_t ret = HANDLE_EINTR(read(kmsg_fd_.get(), buf, kMaxKernelRecord));
  if (ret < 0) {
    if (errno == EPIPE) {
      // This can happen if the kernel buffer is overwritten before we have had
      // a chance to read the messages.  Return true here because the next read
      // should be successful.
      return true;
    }

    // Otherwise log the error if it's not just that the read would block and
    // stop trying to read more data.
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
      PLOG(ERROR) << "Failed to read from kernel log buffer";
    }
    return false;
  }

  if (ret == 0) {
    // End of file.  Realistically should never happen.
    LOG(WARNING) << "Reached end of file for kernel log buffer";
    kmsg_controller_.StopWatchingFileDescriptor();
    return false;
  }

  // Try to parse the record to see if it is something we should try to forward.
  auto* record = pb::Arena::CreateMessage<vm_tools::LogRecord>(&arena_);
  uint64_t sequence = 0;
  if (!ParseKernelRecord(buf, ret, boot_time_, record, &sequence)) {
    // Don't log anything here because it may just be a message we don't care
    // about (like a context line).  Return true because there may still be
    // more data to read from the buffer.
    return true;
  }

  if (sequence <= kernel_sequence_ && kernel_sequence_ != 0) {
    // This message has already been sent to the LogCollector service.
    return true;
  }

  // Update the last read kernel log sequence number.
  kernel_sequence_ = sequence;

  // Update the buffered data counter.
  buffered_size_ += record->ByteSizeLong();

  // Safe because |record| was created by the same arena that owns
  // |kmsg_requset_|.
  kmsg_request_->add_records()->UnsafeArenaSwap(record);

  return true;
}

std::unique_ptr<Collector> Collector::CreateForTesting(
    base::ScopedFD syslog_fd,
    base::ScopedFD kmsg_fd,
    base::Time boot_time,
    std::unique_ptr<vm_tools::LogCollector::Stub> stub) {
  CHECK(stub);
  CHECK(syslog_fd.is_valid());
  CHECK(kmsg_fd.is_valid());
  auto collector = base::WrapUnique<Collector>(new Collector(base::Closure()));

  if (!collector->InitForTesting(std::move(syslog_fd), std::move(kmsg_fd),
                                 boot_time, std::move(stub))) {
    collector.reset();
  }

  return collector;
}

bool Collector::InitForTesting(
    base::ScopedFD syslog_fd,
    base::ScopedFD kmsg_fd,
    base::Time boot_time,
    std::unique_ptr<vm_tools::LogCollector::Stub> stub) {
  // Set the fake boot time.
  boot_time_ = boot_time;

  // Start listening on the syslog socket.
  syslog_fd_.swap(syslog_fd);

  bool ret = base::MessageLoopForIO::current()->WatchFileDescriptor(
      syslog_fd_.get(), true /* persistent */,
      base::MessageLoopForIO::WATCH_READ, &syslog_controller_, this);
  if (!ret) {
    LOG(ERROR) << "Failed to watch syslog file descriptor";
    return false;
  }

  // Start listening on the kernel socket.
  kmsg_fd_.swap(kmsg_fd);
  ret = base::MessageLoopForIO::current()->WatchFileDescriptor(
      kmsg_fd_.get(), true /* persistent */, base::MessageLoopForIO::WATCH_READ,
      &kmsg_controller_, this);
  if (!ret) {
    LOG(ERROR) << "Failed to watch kernel file descriptor";
    return false;
  }
  // Store the stub for the LogCollector.
  stub_ = std::move(stub);

  // Create a temporary file for the kernel log sequence number.
  kernel_sequence_fd_.reset(
      open("/tmp", O_WRONLY | O_TMPFILE | O_CLOEXEC | O_EXCL, 0600));
  if (!kernel_sequence_fd_.is_valid()) {
    PLOG(ERROR) << "Failed to create temporary file for kernel log sequence";
    return false;
  }
  kernel_sequence_ = 0;

  // Start a timer to periodically flush logs.
  timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kTimerFlushMillisecondsForTesting),
      base::Bind(&Collector::FlushLogs, weak_factory_.GetWeakPtr()));

  // Start a new log request buffer.
  syslog_request_ = pb::Arena::CreateMessage<vm_tools::LogRequest>(&arena_);
  kmsg_request_ = pb::Arena::CreateMessage<vm_tools::LogRequest>(&arena_);
  buffered_size_ = 0;

  // Everything succeeded.
  return true;
}

}  // namespace syslog
}  // namespace vm_tools
