// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_SYSLOG_COLLECTOR_H_
#define VM_TOOLS_SYSLOG_COLLECTOR_H_

#include <memory>

#include <base/callback.h>
#include <base/files/file_descriptor_watcher_posix.h>
#include <base/files/scoped_file.h>
#include <base/macros.h>
#include <base/memory/weak_ptr.h>
#include <base/time/time.h>
#include <base/timer/timer.h>
#include <google/protobuf/arena.h>
#include <vm_protos/proto_bindings/vm_host.grpc.pb.h>

namespace vm_tools {
namespace syslog {

// Responsible for listening on /dev/log for any userspace applications that
// wish to log messages with the system syslog.  TODO(chirantan):  This
// currently doesn't handle kernel oops or flushing during shutdown.
class Collector {
 public:
  // Create a new, initialized Collector.
  static std::unique_ptr<Collector> Create(base::Closure shutdown_closure);
  ~Collector() = default;

  static std::unique_ptr<Collector> CreateForTesting(
      base::ScopedFD syslog_fd,
      base::Time boot_time,
      std::unique_ptr<vm_tools::LogCollector::Stub> stub);

 private:
  // Private default constructor.  Use the static factory function to create new
  // instances of this class.
  explicit Collector(base::Closure shutdown_closure);

  // Initializes this Collector.  Starts listening on the syslog socket
  // and sets up timers to periodically flush logs out.
  bool Init();

  // Called periodically to flush any logs that have been buffered.
  void FlushLogs();

  // Reads one log record from the socket and adds it to |syslog_request_|.
  // Returns true if there may still be more data to read from the socket.
  bool ReadOneSyslogRecord();

  // Initializes this Collector for tests.  Starts listening on the
  // provided file descriptor instead of creating a socket and binding to a
  // path on the file system.
  bool InitForTesting(base::ScopedFD syslog_fd,
                      base::Time boot_time,
                      std::unique_ptr<vm_tools::LogCollector::Stub> stub);

  // Called when |syslog_fd_| becomes readable.
  void OnSyslogReadable();

  // Called when |signal_fd_| becomes readable.
  void OnSignalReadable();

  // File descriptor bound to /dev/log.
  base::ScopedFD syslog_fd_;
  std::unique_ptr<base::FileDescriptorWatcher::Controller> syslog_controller_;

  // File descriptor for receiving signals.
  base::ScopedFD signal_fd_;
  std::unique_ptr<base::FileDescriptorWatcher::Controller> signal_controller_;

  // Closure for stopping the MessageLoop.  Posted to the thread's TaskRunner
  // when this program receives a SIGTERM.
  base::Closure shutdown_closure_;

  // Time that the VM booted.  Used to convert kernel timestamps to localtime.
  base::Time boot_time_;

  // Shared arena used for allocating log records.
  google::protobuf::Arena arena_;

  // Non-owning pointer to the current syslog LogRequest.  Owned by arena_.
  vm_tools::LogRequest* syslog_request_;

  // Size of all the currently buffered log records.
  size_t buffered_size_;

  // Connection to the LogCollector service on the host.
  std::unique_ptr<vm_tools::LogCollector::Stub> stub_;

  // Timer used for periodically flushing buffered log records.
  base::RepeatingTimer timer_;

  base::WeakPtrFactory<Collector> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Collector);
};

}  // namespace syslog
}  // namespace vm_tools

#endif  // VM_TOOLS_SYSLOG_COLLECTOR_H_
