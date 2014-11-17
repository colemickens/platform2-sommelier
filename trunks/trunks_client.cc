// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// trunks_client is a command line tool that supports various TPM operations. It
// does not provide direct access to the trunksd D-Bus interface.

#include <stdio.h>
#include <string>

#include <base/command_line.h>
#include <base/logging.h>
#include <chromeos/syslog_logging.h>

#include "trunks/error_codes.h"
#include "trunks/tpm_utility.h"
#include "trunks/trunks_factory_impl.h"

namespace {

void PrintUsage() {
  puts("Options:");
  puts("  --help - Prints this message.");
  puts("  --startup - Performs startup and self-tests.");
  puts("  --clear - Clears the TPM. Use before initializing the TPM.");
  puts("  --init_tpm - Initializes a TPM as CrOS firmware does.");
}

int Startup() {
  trunks::TrunksFactoryImpl factory;
  return factory.GetTpmUtility()->Startup();
}

int Clear() {
  trunks::TrunksFactoryImpl factory;
  return factory.GetTpmUtility()->Clear();
}

int InitializeTpm() {
  trunks::TrunksFactoryImpl factory;
  return factory.GetTpmUtility()->InitializeTpm();
}

}  // namespace


int main(int argc, char **argv) {
  CommandLine::Init(argc, argv);
  chromeos::InitLog(chromeos::kLogToSyslog | chromeos::kLogToStderr);
  CommandLine *cl = CommandLine::ForCurrentProcess();
  if (cl->HasSwitch("startup")) {
    return Startup();
  }
  if (cl->HasSwitch("clear")) {
    return Clear();
  }
  if (cl->HasSwitch("init_tpm")) {
    return InitializeTpm();
  }
  if (cl->HasSwitch("help")) {
    puts("Trunks Client: A command line tool to access the TPM.");
    PrintUsage();
    return 0;
  }
  puts("Invalid options!");
  PrintUsage();
  return -1;
}
