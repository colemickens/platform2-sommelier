// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/socket.h>

#include <linux/vm_sockets.h>  // Needs to come after sys/socket.h

#include <base/at_exit.h>
#include <base/command_line.h>
#include <base/strings/stringprintf.h>
#include <grpc++/grpc++.h>

#include "vm_tools/maitred/service_impl.h"

namespace {

// Default port for maitred.
constexpr unsigned int kPort = 8888;

}  // namespace

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  base::CommandLine::Init(argc, argv);

  grpc::ServerBuilder builder;
  builder.AddListeningPort(
      base::StringPrintf("vsock:%u:%u", VMADDR_CID_ANY, kPort),
      grpc::InsecureServerCredentials());

  vm_tools::maitred::ServiceImpl maitred_service;
  builder.RegisterService(&maitred_service);

  std::unique_ptr<grpc::Server> server = builder.BuildAndStart();
  CHECK(server);

  LOG(INFO) << "Server listening on port " << kPort;

  // The following call will never return.
  server->Wait();

  return 0;
}
