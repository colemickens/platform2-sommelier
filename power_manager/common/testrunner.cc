// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop.h"

int main(int argc, char** argv) {
  CommandLine::Init(argc, argv);
  logging::InitLogging(NULL,
                       logging::LOG_ONLY_TO_SYSTEM_DEBUG_LOG,
                       logging::DONT_LOCK_LOG_FILE,
                       logging::APPEND_TO_OLD_LOG_FILE,
                       logging::ENABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS);
  base::AtExitManager at_exit_manager;
  MessageLoopForIO message_loop;
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
