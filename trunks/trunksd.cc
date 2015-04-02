// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/at_exit.h>
#include <base/command_line.h>
#include <base/message_loop/message_loop.h>
#include <base/threading/thread.h>
#include <chromeos/libminijail.h>
#include <chromeos/minijail/minijail.h>
#include <chromeos/syslog_logging.h>

#include "trunks/background_command_transceiver.h"
#include "trunks/tpm_handle.h"
#include "trunks/trunks_service.h"

namespace {

const uid_t kTrunksUID = 251;
const uid_t kRootUID = 0;
const char kTrunksUser[] = "trunks";
const char kTrunksGroup[] = "trunks";
const char kTrunksSeccompPath[] = "/usr/share/policy/trunksd-seccomp.policy";
const char kBackgroundThreadName[] = "trunksd_background_thread";

void InitMinijailSandbox() {
  CHECK_EQ(getuid(), kRootUID) << "Trunks Daemon not initialized as root.";
  chromeos::Minijail* minijail = chromeos::Minijail::GetInstance();
  struct minijail* jail = minijail->New();
  minijail->DropRoot(jail, kTrunksUser, kTrunksGroup);
  minijail->UseSeccompFilter(jail, kTrunksSeccompPath);
  minijail->Enter(jail);
  minijail->Destroy(jail);
  CHECK_EQ(getuid(), kTrunksUID)
      << "Trunks Daemon was not able to drop to trunks user.";
}

}  // namespace

int main(int argc, char **argv) {
  base::CommandLine::Init(argc, argv);
  chromeos::InitLog(chromeos::kLogToSyslog | chromeos::kLogToStderr);
  base::AtExitManager at_exit_manager;
  // A main message loop.
  base::MessageLoopForIO message_loop;
  // A thread for executing TPM commands.
  base::Thread background_thread(kBackgroundThreadName);
  CHECK(background_thread.Start());
  trunks::TpmHandle tpm_handle;
  CHECK(tpm_handle.Init());
  trunks::BackgroundCommandTransceiver background_transceiver(
      &tpm_handle,
      background_thread.message_loop_proxy());
  InitMinijailSandbox();
  trunks::TrunksService service(&background_transceiver);
  service.Init();
  LOG(INFO) << "Trunks service started!";
  message_loop.Run();
  return -1;
}
