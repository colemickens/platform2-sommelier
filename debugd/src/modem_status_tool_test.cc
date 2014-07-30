// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "debugd/src/modem_status_tool.h"

using std::string;

namespace debugd {

class ModemStatusToolTest : public testing::Test {
 protected:
  static string CollapseNewLines(const string& input) {
    return ModemStatusTool::CollapseNewLines(input);
  }
};

TEST_F(ModemStatusToolTest, CollapseNewLines) {
  EXPECT_EQ("", CollapseNewLines(""));
  EXPECT_EQ("  \nATZ\nOK\n ERROR\n ",
            CollapseNewLines("  \n\rATZ\rOK\r\n ERROR\n\r\n\r "));
}

}  // namespace debugd
