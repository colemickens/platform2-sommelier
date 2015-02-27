// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/command_line.h>
#include <chromeos/flag_helper.h>
#include <chromeos/syslog_logging.h>

#include "germ/launcher.h"

int main(int argc, char** argv) {
  DEFINE_string(name, "",
                "Name of the service, cannot be empty.");
  DEFINE_string(executable, "",
                "Full path of the executable, cannot be empty.");
  chromeos::FlagHelper::Init(argc, argv, "germ");
  chromeos::InitLog(chromeos::kLogToSyslog);

  germ::Launcher launcher;
  return launcher.Run(FLAGS_name, FLAGS_executable);
}
