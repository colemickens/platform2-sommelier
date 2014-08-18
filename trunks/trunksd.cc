// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/at_exit.h>
#include <base/command_line.h>
#include <base/message_loop/message_loop.h>
#include <chromeos/syslog_logging.h>

#include "trunks/tpm_handle_impl.h"
#include "trunks/trunks_service.h"


int main(int argc, char **argv) {
  CommandLine::Init(argc, argv);
  chromeos::InitLog(chromeos::kLogToSyslog | chromeos::kLogToStderr);
  base::AtExitManager at_exit_manager;
  base::MessageLoopForIO message_loop;
  trunks::TrunksService service;
  trunks::TpmHandleImpl* tpm = new trunks::TpmHandleImpl();
  service.Init(tpm);
  LOG(INFO) << "Trunks service started!";
  message_loop.Run();
  return -1;
}
