// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <brillo/flag_helper.h>
#include <brillo/syslog_logging.h>

#include "bluetooth/common/dbus_daemon.h"
#include "bluetooth/common/util.h"
#include "bluetooth/newblued/newblue.h"
#include "bluetooth/newblued/newblue_daemon.h"

int main(int argc, char** argv) {
  brillo::FlagHelper::Init(argc, argv,
                           "newblued, the Chromium OS Newblue daemon.");

  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderrIfTty);

  bool is_idle_mode = !bluetooth::IsBleSplitterEnabled();

  bluetooth::DBusDaemon daemon(std::make_unique<bluetooth::NewblueDaemon>(
      std::make_unique<bluetooth::Newblue>(
          std::make_unique<bluetooth::LibNewblue>()),
      is_idle_mode));
  return daemon.Run();
}
