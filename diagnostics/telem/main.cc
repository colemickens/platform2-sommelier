// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/logging.h>
#include <base/message_loop/message_loop.h>
#include <base/run_loop.h>
#include <brillo/flag_helper.h>
#include <brillo/syslog_logging.h>

#include "diagnostics/grpc_async_adapter/async_grpc_client.h"
#include "diagnosticsd.grpc.pb.h"  // NOLINT(build/include)

namespace {
// URI on which the gRPC interface exposed by the diagnosticsd daemon is
// listening.
constexpr char kDiagnosticsdGrpcUri[] =
    "unix:/run/diagnostics/grpc_sockets/diagnosticsd_socket";
}  // namespace

int main(int argc, char** argv) {
  brillo::FlagHelper::Init(argc, argv, "telem - Device telemetry tool.");

  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderrIfTty);

  base::MessageLoopForIO message_loop_;
  // Allows making outgoing requests to the gRPC interface exposed by the
  // diagnosticsd daemon.
  std::unique_ptr<
      diagnostics::AsyncGrpcClient<diagnostics::grpc_api::Diagnosticsd>>
      diagnosticsd_grpc_client_;

  // Create the AsyncGrpcClient.
  diagnosticsd_grpc_client_ = std::make_unique<
      diagnostics::AsyncGrpcClient<diagnostics::grpc_api::Diagnosticsd>>(
      message_loop_.task_runner(), kDiagnosticsdGrpcUri);
  VLOG(0) << "Created gRPC diagnosticsd client on " << kDiagnosticsdGrpcUri;

  // Shut down the AsyncGrpcClient.
  base::RunLoop loop;
  diagnosticsd_grpc_client_->Shutdown(loop.QuitClosure());
  loop.Run();

  return 0;
}
