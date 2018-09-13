// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dbus/mock_bus.h>
#include <gtest/gtest.h>

#include "debugd/src/log_tool.h"

namespace debugd {

class LogToolTest : public testing::Test {
 protected:
  LogToolTest()
      : log_tool_(new dbus::MockBus(dbus::Bus::Options())) {}

  void AnonymizeLogMap(LogTool::LogMap* log_map) {
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

TEST_F(LogToolTest, EnsureUTF8String) {
  // U+1F600 GRINNING FACE
  constexpr const char kGrinningFaceUTF8[] = "\xF0\x9F\x98\x80";
  constexpr const char kGrinningFaceBase64[] = "<base64>: 8J+YgA==";
  EXPECT_EQ(kGrinningFaceUTF8,
            LogTool::EnsureUTF8String(kGrinningFaceUTF8,
                                      LogTool::Encoding::kAutodetect));
  EXPECT_EQ(
      kGrinningFaceUTF8,
      LogTool::EnsureUTF8String(kGrinningFaceUTF8, LogTool::Encoding::kUtf8));
  EXPECT_EQ(
      kGrinningFaceBase64,
      LogTool::EnsureUTF8String(kGrinningFaceUTF8, LogTool::Encoding::kBinary));

  // .xz Stream Header Magic Bytes
  constexpr const char kXzStreamHeaderMagicBytes[] = "\xFD\x37\x7A\x58\x5A\x00";
  constexpr const char kXzStreamHeaderMagicUTF8[] =
      "\xEF\xBF\xBD"
      "7zXZ";
  constexpr const char kXzStreamHeaderMagicBase64[] = "<base64>: /Td6WFo=";
  EXPECT_EQ(kXzStreamHeaderMagicBase64,
            LogTool::EnsureUTF8String(kXzStreamHeaderMagicBytes,
                                      LogTool::Encoding::kAutodetect));
  EXPECT_EQ(kXzStreamHeaderMagicUTF8,
            LogTool::EnsureUTF8String(kXzStreamHeaderMagicBytes,
                                      LogTool::Encoding::kUtf8));
  EXPECT_EQ(kXzStreamHeaderMagicBase64,
            LogTool::EnsureUTF8String(kXzStreamHeaderMagicBytes,
                                      LogTool::Encoding::kBinary));
}

}  // namespace debugd
