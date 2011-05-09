// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>
#include <gflags/gflags.h>
#include <glog/logging.h>

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::SetCommandLineOptionWithMode("min_log_level",
                                       "0",
                                       google::SET_FLAG_IF_DEFAULT);
  google::InitGoogleLogging(argv[0]);
  google::LogToStderr();
  return RUN_ALL_TESTS();
}
