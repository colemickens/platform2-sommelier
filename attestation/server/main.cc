// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <base/at_exit.h>
#include <base/command_line.h>
#include <base/message_loop/message_loop.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <chromeos/syslog_logging.h>

#include "attestation/server/attestation_service.h"

int main(int argc, char* argv[]) {
  CommandLine::Init(argc, argv);

  chromeos::InitLog(chromeos::kLogToSyslog | chromeos::kLogToStderr);

  base::AtExitManager at_exit_manager;  // This is for the message loop.
  base::MessageLoopForIO message_loop;

  attestation::AttestationService service;
  service.Init();

  message_loop.Run();
  return 0;
}
