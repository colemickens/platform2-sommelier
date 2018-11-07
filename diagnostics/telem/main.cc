// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/logging.h>
#include <base/message_loop/message_loop.h>
#include <brillo/flag_helper.h>

#include "diagnostics/telem/telem_connection.h"

namespace {
// URI on which the gRPC interface exposed by the diagnosticsd daemon is
// listening.
constexpr char kDiagnosticsdGrpcUri[] =
    "unix:/run/diagnostics/grpc_sockets/diagnosticsd_socket";
}  // namespace

int main(int argc, char** argv) {
  brillo::FlagHelper::Init(argc, argv, "telem - Device telemetry tool.");

  logging::InitLogging(logging::LoggingSettings());

  base::MessageLoopForIO message_loop;

  diagnostics::TelemConnection telemConnection(kDiagnosticsdGrpcUri);

  telemConnection.Connect();

  telemConnection.GetItem(diagnostics::TelemetryItemEnum::kMemInfo);

  return 0;
}
