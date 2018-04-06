// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
#include <base/bind.h>
#include <base/command_line.h>
#include <base/logging.h>
#include <base/message_loop/message_loop.h>
#include <base/run_loop.h>
#include <base/strings/stringprintf.h>
#include <base/synchronization/waitable_event.h>
#include <base/threading/thread.h>

#include "vm_tools/common/constants.h"
#include "vm_tools/garcon/host_notifier.h"
#include "vm_tools/garcon/service_impl.h"

#include "container_guest.grpc.pb.h"  // NOLINT(build/include)

constexpr char kLogPrefix[] = "garcon: ";
constexpr char kServerSwitch[] = "server";
constexpr char kClientSwitch[] = "client";

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

void RunGarconService(base::WaitableEvent* event,
                      std::shared_ptr<grpc::Server>* server_copy) {
  // We don't want to receive SIGTERM on this thread.
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGTERM);
  sigprocmask(SIG_BLOCK, &mask, nullptr);

  // Build the server.
  grpc::ServerBuilder builder;
  builder.AddListeningPort(base::StringPrintf("[::]:%u", vm_tools::kGarconPort),
                           grpc::InsecureServerCredentials());

  vm_tools::garcon::ServiceImpl garcon_service;
  builder.RegisterService(&garcon_service);

  std::shared_ptr<grpc::Server> server(builder.BuildAndStart().release());

  *server_copy = server;
  event->Signal();

  if (server) {
    LOG(INFO) << "Server listening on port " << vm_tools::kGarconPort;
    // The following call will never return since we have no mechanism for
    // actually shutting down garcon.
    server->Wait();
  }
}

void PrintUsage() {
  LOG(INFO) << "Garcon: VM container bridge for Chrome OS\n\n"
            << "Mode Switches (must use one):\n"
            << "  --server: run in background as daemon\n"
            << "  --client: run as client and send one message to host\n"
            << "Client Switches (only with --client):\n";
}

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  base::MessageLoopForIO message_loop;
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
    // TODO(jkardatzke): We will be adding a client option shortly, so don't
    // remove this code yet.
    LOG(ERROR) << "Missing client switch for client mode.";
    PrintUsage();
    return -1;
  }

  // Set up logging to syslog for server mode.
  openlog(kLogPrefix, LOG_PID, LOG_DAEMON);
  logging::SetLogMessageHandler(LogToSyslog);

  // Thread that the gRPC server is running on.
  base::Thread grpc_thread{"gRPC Thread"};
  if (!grpc_thread.Start()) {
    LOG(ERROR) << "Failed starting the gRPC thread";
    return -1;
  }

  // Launch the gRPC server on the gRPC thread.
  std::shared_ptr<grpc::Server> server_copy;
  base::WaitableEvent event(false /*manual_reset*/,
                            false /*initially_signaled*/);
  bool ret = grpc_thread.task_runner()->PostTask(
      FROM_HERE, base::Bind(&RunGarconService, &event, &server_copy));
  if (!ret) {
    LOG(ERROR) << "Failed to post server startup task to grpc thread";
    return -1;
  }

  // Wait for the gRPC server to start.
  event.Wait();

  if (!server_copy) {
    LOG(ERROR) << "gRPC server failed to start";
    return -1;
  }

  if (signal(SIGCHLD, SIG_IGN) == SIG_ERR) {
    PLOG(ERROR) << "Unable to explicitly ignore SIGCHILD";
    return -1;
  }

  // Now setup the HostNotifier on the run loop for the main thread. It needs to
  // have its own run loop separate from the gRPC server since it will be using
  // base::FilePathWatcher to identify installed application changes.
  base::RunLoop run_loop;

  std::unique_ptr<vm_tools::garcon::HostNotifier> host_notifier =
      vm_tools::garcon::HostNotifier::Create(server_copy,
                                             run_loop.QuitClosure());
  if (!host_notifier) {
    LOG(ERROR) << "Failure setting up the HostNotifier";
    return -1;
  }

  run_loop.Run();

  return 0;
}
