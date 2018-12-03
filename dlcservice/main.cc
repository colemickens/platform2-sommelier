// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <brillo/flag_helper.h>
#include <brillo/syslog_logging.h>

#include "dlcservice/dlc_service.h"

int main(int argc, char** argv) {
  brillo::OpenLog("dlcservice", true);
  brillo::InitLog(brillo::kLogToSyslog);

  DEFINE_bool(load_installed, true, "Load the installed DLC modules.");
  brillo::FlagHelper::Init(argc, argv, "dlcservice");

  return dlcservice::DlcService(FLAGS_load_installed).Run();
}
