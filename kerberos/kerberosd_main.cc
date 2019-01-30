// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/logging.h>
#include <brillo/syslog_logging.h>

int main(int /* argc */, char* /* argv */[]) {
  brillo::OpenLog("kerberosd", true /* log_pid */);
  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderrIfTty);

  return 0;
}
