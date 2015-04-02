// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/at_exit.h>
#include <base/command_line.h>
#include <base/logging.h>
#include <base/message_loop/message_loop.h>
#include <chromeos/syslog_logging.h>
#include <gtest/gtest.h>

int main(int argc, char* argv[]) {
  base::CommandLine::Init(argc, argv);
  chromeos::InitLog(chromeos::kLogToStderr);
  logging::SetMinLogLevel(logging::LOG_WARNING);
  base::AtExitManager at_exit_manager;
  base::MessageLoopForIO message_loop;
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
