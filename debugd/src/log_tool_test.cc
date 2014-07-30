// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "debugd/src/log_tool.h"

namespace debugd {

class LogToolTest : public testing::Test {
 protected:
  void AnonymizeLogMap(LogTool::LogMap *log_map) {
    log_tool_.AnonymizeLogMap(log_map);
  }

  LogTool log_tool_;
};

TEST_F(LogToolTest, AnonymizeLogMap) {
  LogTool::LogMap log_map;
  AnonymizeLogMap(&log_map);
  EXPECT_TRUE(log_map.empty());
  static const char kKey1[] = "log-key1";
  static const char kKey2[] = "log-key2";
  static const char kMAC[] = "aa:bb:cc:dd:ee:ff";
  static const char kAnonymousMAC[] = "aa:bb:cc:00:00:01";
  log_map[kKey1] = kMAC;
  log_map[kKey2] = kMAC;
  AnonymizeLogMap(&log_map);
  EXPECT_EQ(2, log_map.size());
  EXPECT_EQ(kAnonymousMAC, log_map[kKey1]);
  EXPECT_EQ(kAnonymousMAC, log_map[kKey2]);
}

}  // namespace debugd
