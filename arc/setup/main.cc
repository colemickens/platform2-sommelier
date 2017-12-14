// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Make sure to pass (at least) cheets_SELinuxTest, cheets_ContainerMount,
// cheets_DownloadsFilesystem, cheets_FileSystemPermissions, and
// cheets_PerfBoot auto tests.
//
// For unit testing, see arc_setup_util_unittest.cc.

#include <base/at_exit.h>
#include <base/command_line.h>
#include <base/logging.h>

#include "arc_setup.h"  // NOLINT - TODO(b/32971714): fix it properly.

int main(int argc, char** argv) {
  base::AtExitManager at_exit;

  base::CommandLine::Init(argc, argv);
  logging::InitLogging(logging::LoggingSettings());

  arc::ArcSetup setup;
  setup.Run();
  return 0;
}
