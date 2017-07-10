// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_tools/syslog/collector.h"

#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <linux/vm_sockets.h>  // Needs to come after sys/socket.h

#include <string>
#include <utility>

#include <base/bind.h>
#include <base/location.h>
#include <base/logging.h>
#include <base/memory/ptr_util.h>
#include <base/message_loop/message_loop.h>
#include <base/posix/eintr_wrapper.h>
#include <base/strings/stringprintf.h>
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

// Max number of records we should attempt to read out of the socket at a time.
constexpr int kMaxRecordCount = 11;

// Path to the standard syslog listening path.
constexpr char kDevLog[] = "/dev/log";

// Known host port for the LogCollector service.
constexpr unsigned int kLogCollectorPort = 9999;

// Path to the standard empty directory where we will jail the daemon.
constexpr char kVarEmpty[] = "/var/empty";

// Name for the "syslog" user and group.
constexpr char kSyslog[] = "syslog";

}  // namespace

std::unique_ptr<Collector> Collector::Create() {
  auto collector = base::WrapUnique<Collector>(new Collector());

  if (!collector->Init()) {
    collector.reset();
  }

  return collector;
}

void Collector::OnFileCanReadWithoutBlocking(int fd) {
  DCHECK_EQ(fd, syslog_fd_.get());
  bool more = true;
  for (int i = 0; i < kMaxRecordCount && more; ++i) {
    more = ReadOneRecord();
  }
}

void Collector::OnFileCanWriteWithoutBlocking(int fd) {
  NOTREACHED();
}

Collector::Collector() : weak_factory_(this) {}

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

  bool ret = base::MessageLoopForIO::current()->WatchFileDescriptor(
      syslog_fd_.get(), true /* persistent */,
      base::MessageLoopForIO::WATCH_READ, &syslog_controller_, this);
  if (!ret) {
    LOG(ERROR) << "Failed to watch syslog file descriptor";
    return false;
  }

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
  current_request_ = pb::Arena::CreateMessage<vm_tools::LogRequest>(&arena_);
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
  if (current_request_->records_size() <= 0) {
    // Nothing to do.  Just return.
    return;
  }

  grpc::ClientContext ctx;
  vm_tools::EmptyMessage response;
  grpc::Status status =
      stub_->CollectUserLogs(&ctx, *current_request_, &response);

  if (!status.ok()) {
    LOG(ERROR) << "Failed to send user logs to LogCollector service.  Error "
               << "code " << status.error_code() << ": "
               << status.error_message();
  }

  // Reset everything.
  arena_.Reset();
  current_request_ = pb::Arena::CreateMessage<vm_tools::LogRequest>(&arena_);
  buffered_size_ = 0;
}

bool Collector::ReadOneRecord() {
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
  // |current_request_|.
  current_request_->add_records()->UnsafeArenaSwap(record);

  // Send all buffered records immediately if we've crossed the threshold.
  if (buffered_size_ > kBufferThreshold) {
    FlushLogs();
    timer_.Reset();
  }

  return true;
}

std::unique_ptr<Collector> Collector::CreateForTesting(
    base::ScopedFD syslog_fd,
    std::unique_ptr<vm_tools::LogCollector::Stub> stub) {
  CHECK(stub);
  CHECK(syslog_fd.is_valid());
  auto collector = base::WrapUnique<Collector>(new Collector());

  if (!collector->InitForTesting(std::move(syslog_fd), std::move(stub))) {
    collector.reset();
  }

  return collector;
}

bool Collector::InitForTesting(
    base::ScopedFD listen_fd,
    std::unique_ptr<vm_tools::LogCollector::Stub> stub) {
  // Start listening on the syslog socket.
  syslog_fd_.swap(listen_fd);

  bool ret = base::MessageLoopForIO::current()->WatchFileDescriptor(
      syslog_fd_.get(), true /* persistent */,
      base::MessageLoopForIO::WATCH_READ, &syslog_controller_, this);
  if (!ret) {
    LOG(ERROR) << "Failed to watch syslog file descriptor";
    return false;
  }

  // Store the stub for the LogCollector.
  stub_ = std::move(stub);

  // Start a timer to periodically flush logs.
  timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kTimerFlushMillisecondsForTesting),
      base::Bind(&Collector::FlushLogs, weak_factory_.GetWeakPtr()));

  // Start a new log request buffer.
  current_request_ = pb::Arena::CreateMessage<vm_tools::LogRequest>(&arena_);
  buffered_size_ = 0;

  // Everything succeeded.
  return true;
}

}  // namespace syslog
}  // namespace vm_tools
