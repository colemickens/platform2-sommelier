// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Make sure to pass (at least) cheets_SELinuxTest, cheets_ContainerMount,
// cheets_DownloadsFilesystem, cheets_FileSystemPermissions, and
// cheets_PerfBoot auto tests.
//
// For unit testing, see arc_setup_util_unittest.cc.

#include <string>

#include <base/at_exit.h>
#include <base/command_line.h>
#include <base/timer/elapsed_timer.h>
#include <base/logging.h>

#include "arc/setup/arc_setup.h"

int main(int argc, char** argv) {
  base::ElapsedTimer timer;
  base::AtExitManager at_exit;

  base::CommandLine::Init(argc, argv);
  logging::InitLogging(logging::LoggingSettings());

  const std::string command_line =
      base::CommandLine::ForCurrentProcess()->GetCommandLineString();
  LOG(INFO) << "Starting " << command_line;
  {
    arc::ArcSetup setup;
    setup.Run();
  }
  LOG(INFO) << command_line << " took "
            << timer.Elapsed().InMillisecondsRoundedUp() << "ms";
  return 0;
}
