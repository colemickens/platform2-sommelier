// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <arpa/inet.h>
#include <limits.h>
#include <syslog.h>
#include <unistd.h>

#include <string>

// syslog.h and base/logging.h both try to #define LOG_INFO and LOG_WARNING.
// We need to #undef at least these two before including base/logging.h.  The
// others are included to be consistent.
namespace {
const int kSyslogDebug = LOG_DEBUG;
const int kSyslogInfo = LOG_INFO;
const int kSyslogWarning = LOG_WARNING;
const int kSyslogError = LOG_ERR;
const int kSyslogCritical = LOG_CRIT;

#undef LOG_INFO
#undef LOG_WARNING
#undef LOG_ERR
#undef LOG_CRIT
}  // namespace

#include <base/at_exit.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/strings/stringprintf.h>
#include <base/strings/string_util.h>
#include <grpc++/grpc++.h>

#include "vm_tools/common/constants.h"

#include "container_host.grpc.pb.h"  // NOLINT(build/include)

constexpr char kHostIpFile[] = "/dev/.host_ip";
constexpr char kLogPrefix[] = "garcon: ";

bool LogToSyslog(logging::LogSeverity severity,
                 const char* /* file */,
                 int /* line */,
                 size_t message_start,
                 const std::string& message) {
  switch (severity) {
    case logging::LOG_INFO:
      severity = kSyslogInfo;
      break;
    case logging::LOG_WARNING:
      severity = kSyslogWarning;
      break;
    case logging::LOG_ERROR:
      severity = kSyslogError;
      break;
    case logging::LOG_FATAL:
      severity = kSyslogCritical;
      break;
    default:
      severity = kSyslogDebug;
      break;
  }
  syslog(severity, "%s", message.c_str() + message_start);

  return true;
}

bool NotifyHostGarconIsReady() {
  // Notify the host system that we are ready.
  char host_addr[INET_ADDRSTRLEN + 1];
  base::FilePath host_ip_path(kHostIpFile);
  int num_read = base::ReadFile(host_ip_path, host_addr, sizeof(host_addr) - 1);
  if (num_read <= 0) {
    LOG(ERROR) << "Failed reading the host IP from: "
               << host_ip_path.MaybeAsASCII();
    return false;
  }
  host_addr[num_read] = '\0';
  vm_tools::container::ContainerListener::Stub stub(grpc::CreateChannel(
      base::StringPrintf("%s:%u", host_addr, vm_tools::kGarconPort),
      grpc::InsecureChannelCredentials()));
  grpc::ClientContext ctx;
  vm_tools::container::ContainerStartupInfo startup_info;
  char buf[HOST_NAME_MAX + 1];
  if (gethostname(buf, sizeof(buf)) < 0) {
    PLOG(ERROR) << "Failed getting the hostname of the container";
    return false;
  }
  startup_info.set_name(buf);
  vm_tools::EmptyMessage empty;
  grpc::Status status = stub.ContainerReady(&ctx, startup_info, &empty);
  if (!status.ok()) {
    LOG(WARNING) << "Failed to notify host system that container is ready: "
                 << status.error_message();
    return false;
  }
  return true;
}

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  logging::InitLogging(logging::LoggingSettings());

  // Set up logging to syslog.
  openlog(kLogPrefix, LOG_PID, LOG_DAEMON);
  logging::SetLogMessageHandler(LogToSyslog);

  // All we are doing for now is notifying the host the container is started up
  // and we are ready to receive messages (once we have that part implemented).
  if (!NotifyHostGarconIsReady()) {
    LOG(ERROR) << "Failed notifying host that container is ready";
  }

  return 0;
}
