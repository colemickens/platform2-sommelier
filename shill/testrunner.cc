// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>
//#include <base/command_line.h>
#include "shill/shill_logging.h"

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  google::InitGoogleLogging(argv[0]);
  google::LogToStderr();
  shill::Log::Enable(SHILL_LOG_ALL);
  return RUN_ALL_TESTS();
}
