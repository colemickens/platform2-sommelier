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
#include <base/command_line.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/strings/stringprintf.h>
#include <base/strings/string_util.h>
#include <grpc++/grpc++.h>

#include "vm_tools/common/constants.h"

#include "container_host.grpc.pb.h"  // NOLINT(build/include)

constexpr char kHostIpFile[] = "/dev/.host_ip";
constexpr char kSecurityTokenFile[] = "/dev/.container_token";
constexpr char kLogPrefix[] = "garcon: ";
constexpr int kSecurityTokenLength = 36;
constexpr char kServerSwitch[] = "server";
constexpr char kClientSwitch[] = "client";
constexpr char kShutdownSwitch[] = "shutdown";

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

std::string GetHostIp() {
  char host_addr[INET_ADDRSTRLEN + 1];
  base::FilePath host_ip_path(kHostIpFile);
  int num_read = base::ReadFile(host_ip_path, host_addr, sizeof(host_addr) - 1);
  if (num_read <= 0) {
    LOG(ERROR) << "Failed reading the host IP from: "
               << host_ip_path.MaybeAsASCII();
    return "";
  }
  host_addr[num_read] = '\0';
  return std::string(host_addr);
}

std::string GetSecurityToken() {
  char token[kSecurityTokenLength + 1];
  base::FilePath security_token_path(kSecurityTokenFile);
  int num_read = base::ReadFile(security_token_path, token, sizeof(token) - 1);
  if (num_read <= 0) {
    LOG(ERROR) << "Failed reading the container token from: "
               << security_token_path.MaybeAsASCII();
    return "";
  }
  token[num_read] = '\0';
  return std::string(token);
}

bool NotifyHostGarconIsReady() {
  // Notify the host system that we are ready.
  std::string host_ip = GetHostIp();
  std::string token = GetSecurityToken();
  if (token.empty() || host_ip.empty()) {
    return false;
  }
  vm_tools::container::ContainerListener::Stub stub(grpc::CreateChannel(
      base::StringPrintf("%s:%u", host_ip.c_str(), vm_tools::kGarconPort),
      grpc::InsecureChannelCredentials()));
  grpc::ClientContext ctx;
  vm_tools::container::ContainerStartupInfo startup_info;
  startup_info.set_token(token);
  vm_tools::EmptyMessage empty;
  grpc::Status status = stub.ContainerReady(&ctx, startup_info, &empty);
  if (!status.ok()) {
    LOG(WARNING) << "Failed to notify host system that container is ready: "
                 << status.error_message();
    return false;
  }
  return true;
}

bool NotifyHostOfContainerShutdown() {
  // Notify the host system that we are ready.
  std::string host_ip = GetHostIp();
  std::string token = GetSecurityToken();
  if (token.empty() || host_ip.empty()) {
    return false;
  }
  vm_tools::container::ContainerListener::Stub stub(grpc::CreateChannel(
      base::StringPrintf("%s:%u", host_ip.c_str(), vm_tools::kGarconPort),
      grpc::InsecureChannelCredentials()));
  grpc::ClientContext ctx;
  vm_tools::container::ContainerShutdownInfo shutdown_info;
  shutdown_info.set_token(token);
  vm_tools::EmptyMessage empty;
  grpc::Status status = stub.ContainerShutdown(&ctx, shutdown_info, &empty);
  if (!status.ok()) {
    LOG(WARNING) << "Failed to notify host system that container is shutting "
                 << "down: " << status.error_message();
    return false;
  }
  return true;
}

void PrintUsage() {
  LOG(INFO) << "Garcon: VM container bridge for Chrome OS\n\n"
            << "Mode Switches (must use one):\n"
            << "  --server: run in background as daemon\n"
            << "  --client: run as client and send one message to host\n"
            << "Client Switches (only with --client):\n"
            << "  --shutdown: inform the host the container is shutting down\n";
}

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  base::CommandLine::Init(argc, argv);
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  logging::InitLogging(logging::LoggingSettings());

  bool serverMode = cl->HasSwitch(kServerSwitch);
  bool clientMode = cl->HasSwitch(kClientSwitch);
  // The standard says that bool to int conversion is implicit and that
  // false => 0 and true => 1.
  // clang-format off
  if (serverMode + clientMode != 1) {
    // clang-format on
    LOG(ERROR) << "Exactly one of --server or --client must be used.";
    PrintUsage();
    return -1;
  }

  if (clientMode) {
    if (cl->HasSwitch(kShutdownSwitch)) {
      if (!NotifyHostOfContainerShutdown()) {
        return -1;
      }
    } else {
      LOG(ERROR) << "Missing client switch for client mode.";
      PrintUsage();
      return -1;
    }
    return 0;
  }

  // Set up logging to syslog for server mode.
  openlog(kLogPrefix, LOG_PID, LOG_DAEMON);
  logging::SetLogMessageHandler(LogToSyslog);

  // All we are doing for now is notifying the host the container is started up
  // and we are ready to receive messages (once we have that part implemented).
  if (!NotifyHostGarconIsReady()) {
    LOG(ERROR) << "Failed notifying host that container is ready";
  }

  return 0;
}
