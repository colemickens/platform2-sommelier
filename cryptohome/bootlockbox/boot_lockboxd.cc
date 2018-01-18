// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <brillo/syslog_logging.h>

#include "cryptohome/bootlockbox/boot_lockbox_service.h"

int main(int argc, char** argv) {
  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderr);

  cryptohome::BootLockboxService service;
  service.Run();
  return 0;
}
