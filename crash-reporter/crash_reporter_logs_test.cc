// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <base/files/file_path.h>
#include <brillo/key_value_store.h>
#include <gtest/gtest.h>

#include "crash-reporter/test_util.h"

namespace {

// Name of the checked-in configuration file containing log-collection commands.
const char kLogConfigFileName[] = "crash_reporter_logs.conf";

// Executable name for Chrome. kLogConfigFileName is expected to contain
// this entry.
const char kChromeExecName[] = "chrome";

}  // namespace

// Tests that the config file is parsable and that Chrome is listed.
TEST(CrashReporterLogsTest, ReadConfig) {
  brillo::KeyValueStore store;
  ASSERT_TRUE(store.Load(test_util::GetTestDataPath(kLogConfigFileName)));
  std::string command;
  EXPECT_TRUE(store.GetString(kChromeExecName, &command));
  EXPECT_FALSE(command.empty());
}
