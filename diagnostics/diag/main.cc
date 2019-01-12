// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <iostream>
#include <vector>

#include <base/logging.h>
#include <base/message_loop/message_loop.h>

#include "diagnostics/diag/diag_routine_requester.h"

namespace {
// URI on which the gRPC interface exposed by the diagnosticsd daemon is
// listening.
constexpr char kDiagnosticsdGrpcUri[] =
    "unix:/run/diagnostics/grpc_sockets/diagnosticsd_socket";
}  // namespace

// 'diag' command-line tool:
//
// Test driver for libdiag. Only supports running a single diagnostic routine
// at a time.
int main(int argc, char** argv) {
  logging::InitLogging(logging::LoggingSettings());

  base::MessageLoopForIO message_loop;

  diagnostics::DiagRoutineRequester routine_requester;

  routine_requester.Connect(kDiagnosticsdGrpcUri);

  std::vector<diagnostics::grpc_api::GetAvailableRoutinesResponse::Routine>
      available_routines = routine_requester.GetAvailableRoutines();

  if (available_routines.empty()) {
    std::cout << "Failed to retrieve available routines." << std::endl;
    return EXIT_FAILURE;
  }

  for (auto routine : available_routines)
    std::cout << "Available routine: " << routine << std::endl;

  return EXIT_SUCCESS;
}
