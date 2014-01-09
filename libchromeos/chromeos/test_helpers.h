// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _CHROMEOS_TEST_HELPERS_H_
#define _CHROMEOS_TEST_HELPERS_H_

#include "gtest/gtest.h"

#include <string>

#include <base/command_line.h>
#include <base/file_util.h>
#include <base/logging.h>
#include <chromeos/syslog_logging.h>

#if BASE_VER >= 242728
#include <base/files/file_path.h>
#endif

#if BASE_VER >= 242728
using base::FilePath;
using base::ReadFileToString;
#else
using file_util::ReadFileToString;
#endif

inline void ExpectFileEquals(const char* golden,
                             const char* file_path) {
  std::string contents;
  EXPECT_TRUE(ReadFileToString(FilePath(file_path), &contents));
  EXPECT_EQ(golden, contents);
}

inline void SetUpTests(int *argc, char** argv, bool log_to_stderr) {
  CommandLine::Init(*argc, argv);
  ::chromeos::InitLog(log_to_stderr ? chromeos::kLogToStderr : 0);
  ::chromeos::LogToString(true);
  ::testing::InitGoogleTest(argc, argv);
}

#endif  // _CHROMEOS_TEST_HELPERS_H_
