// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <brillo/syslog_logging.h>

#include "dlcservice/dlc_service.h"

int main(int argc, char** argv) {
  brillo::OpenLog("dlcservice", true);
  brillo::InitLog(brillo::kLogToSyslog);

  return dlcservice::DlcService().Run();
}
