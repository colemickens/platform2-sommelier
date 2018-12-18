// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "init/file_attrs_cleaner.h"

#include <sysexits.h>

#include <base/files/file_path.h>
#include <base/logging.h>
#include <brillo/flag_helper.h>
#include <brillo/syslog_logging.h>

using file_attrs_cleaner::ScanDir;

int main(int argc, char* argv[]) {
  brillo::FlagHelper::Init(argc, argv, "Chromium OS File Attrs Cleaner");
  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderrIfTty);

  if (argc <= 1) {
    LOG(ERROR) << "Need at least one directory to scan.";
    return EX_USAGE;
  }

  bool ret = true;
  for (int i = 1; i < argc; ++i)
    ret &= ScanDir(argv[i]);
  return ret ? EX_OK : 1;
}
