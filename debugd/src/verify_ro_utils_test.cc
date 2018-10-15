// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/verify_ro_utils.h"

#include <string>

#include <gtest/gtest.h>

namespace debugd {

class VerifyRoUtilsTest : public testing::Test {
};

TEST_F(VerifyRoUtilsTest, GetLinesWithKeysSuccess) {
  std::string message("KEY_A=111\nKEY_B=222\nKEY_C=333\n");
  std::string result =
      GetKeysValuesFromProcessOutput(message, {"KEY_A", "KEY_C"});

  EXPECT_EQ(result, "KEY_A=111\nKEY_C=333\n");
}

TEST_F(VerifyRoUtilsTest, GetLinesWithKeysError) {
  std::string message("KEY_A=111\nKEY_B=222\nKEY_C=333\n");
  std::string result =
      GetKeysValuesFromProcessOutput(message, {"KEY_A", "BAD_KEY"});

  EXPECT_EQ(result, "<invalid process output>");
}

}  // namespace debugd
