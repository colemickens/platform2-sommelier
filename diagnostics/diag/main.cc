// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <iostream>
#include <vector>

#include <base/logging.h>
#include <base/message_loop/message_loop.h>

#include "diagnostics/constants/grpc_constants.h"
#include "diagnostics/diag/diag_routine_requester.h"


// 'diag' command-line tool:
//
// Test driver for libdiag. Only supports running a single diagnostic routine
// at a time.
int main(int argc, char** argv) {
  logging::InitLogging(logging::LoggingSettings());

  base::MessageLoopForIO message_loop;

  diagnostics::DiagRoutineRequester routine_requester;

  routine_requester.Connect(diagnostics::kWilcoDtcSupportdGrpcUri);

  std::vector<diagnostics::grpc_api::DiagnosticRoutine> available_routines =
      routine_requester.GetAvailableRoutines();

  if (available_routines.empty()) {
    std::cout << "Failed to retrieve available routines." << std::endl;
    return EXIT_FAILURE;
  }

  for (auto routine : available_routines)
    std::cout << "Available routine: " << routine << std::endl;

  return EXIT_SUCCESS;
}
