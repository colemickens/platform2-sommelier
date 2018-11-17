// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <linux/vm_sockets.h>

#include <memory>

#include <base/at_exit.h>
#include <base/files/scoped_file.h>
#include <base/logging.h>
#include <base/strings/stringprintf.h>
#include <brillo/flag_helper.h>
#include <brillo/syslog_logging.h>
#include <grpcpp/grpcpp.h>

#include "vm_host.grpc.pb.h"  // NOLINT(build/include)
#include "vm_tools/syslog/forwarder.h"

namespace {
constexpr unsigned int kPort = 9999;
}

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderrIfTty);

  DEFINE_string(
      log_destination, "/dev/log",
      "Path to unix domain datagram socket to which logs will be forwarded");
  brillo::FlagHelper::Init(argc, argv, "VM log forwarding tool");

  base::ScopedFD dest(socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0));
  if (!dest.is_valid()) {
    PLOG(ERROR) << "Failed to create unix domain datagram socket";
    return EXIT_FAILURE;
  }

  struct sockaddr_un un = {
      .sun_family = AF_UNIX,
  };
  if (FLAGS_log_destination.size() >= sizeof(un.sun_path)) {
    LOG(ERROR) << "Requested log destination path (" << FLAGS_log_destination
               << ") is too long.  Maximum path length: " << sizeof(un.sun_path)
               << " characters";
    return EXIT_FAILURE;
  }

  // sun_path is zero-initialized above so we just need to copy the path.
  memcpy(un.sun_path, FLAGS_log_destination.c_str(),
         FLAGS_log_destination.size());

  if (connect(dest.get(), reinterpret_cast<struct sockaddr*>(&un),
              sizeof(un)) != 0) {
    PLOG(ERROR) << "Failed to connect to " << FLAGS_log_destination;
    return EXIT_FAILURE;
  }

  vm_tools::syslog::Forwarder forwarder(std::move(dest));

  grpc::ServerBuilder builder;
  builder.AddListeningPort(
      base::StringPrintf("vsock:%u:%u", VMADDR_CID_ANY, kPort),
      grpc::InsecureServerCredentials());
  builder.RegisterService(&forwarder);

  std::unique_ptr<grpc::Server> server = builder.BuildAndStart();
  CHECK(server);

  LOG(INFO) << "VM log forwarder listening on port " << kPort;

  server->Wait();

  return EXIT_SUCCESS;
}
