// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/at_exit.h>
#include <base/command_line.h>
#include <base/logging.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  logging::InitLogging(logging::LoggingSettings());

  base::AtExitManager exit_manager;
  testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}
