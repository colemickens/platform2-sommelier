// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <grp.h>
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#include <string>
#include <vector>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <dbus/mock_bus.h>
#include <gtest/gtest.h>

#include "debugd/src/log_tool.h"

namespace {
bool WriteFile(const base::FilePath& path, const std::string& contents) {
  return base::CreateDirectory(path.DirName()) &&
         base::WriteFile(path, contents.c_str(), contents.length()) ==
             contents.length();
}
}  // namespace

namespace debugd {

class LogToolTest : public testing::Test {
 protected:
  LogToolTest() : log_tool_(new dbus::MockBus(dbus::Bus::Options())) {}

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
  static const char kAnonymousMAC[] = "[MAC OUI=aa:bb:cc IFACE=1]";
  log_map[kKey1] = kMAC;
  log_map[kKey2] = kMAC;
  AnonymizeLogMap(&log_map);
  EXPECT_EQ(2, log_map.size());
  EXPECT_EQ(kAnonymousMAC, log_map[kKey1]);
  EXPECT_EQ(kAnonymousMAC, log_map[kKey2]);
}

TEST_F(LogToolTest, EncodeString) {
  // U+1F600 GRINNING FACE
  constexpr const char kGrinningFaceUTF8[] = "\xF0\x9F\x98\x80";
  constexpr const char kGrinningFaceBase64[] = "<base64>: 8J+YgA==";
  EXPECT_EQ(kGrinningFaceUTF8,
            LogTool::EncodeString(kGrinningFaceUTF8,
                                  LogTool::Encoding::kAutodetect));
  EXPECT_EQ(
      kGrinningFaceUTF8,
      LogTool::EncodeString(kGrinningFaceUTF8, LogTool::Encoding::kUtf8));
  EXPECT_EQ(
      kGrinningFaceBase64,
      LogTool::EncodeString(kGrinningFaceUTF8, LogTool::Encoding::kBase64));

  // .xz Stream Header Magic Bytes
  constexpr const char kXzStreamHeaderMagicBytes[] = "\xFD\x37\x7A\x58\x5A\x00";
  constexpr const char kXzStreamHeaderMagicUTF8[] =
      "\xEF\xBF\xBD"
      "7zXZ";
  constexpr const char kXzStreamHeaderMagicBase64[] = "<base64>: /Td6WFo=";
  EXPECT_EQ(kXzStreamHeaderMagicBase64,
            LogTool::EncodeString(kXzStreamHeaderMagicBytes,
                                  LogTool::Encoding::kAutodetect));
  EXPECT_EQ(kXzStreamHeaderMagicUTF8,
            LogTool::EncodeString(kXzStreamHeaderMagicBytes,
                                  LogTool::Encoding::kUtf8));
  EXPECT_EQ(kXzStreamHeaderMagicBase64,
            LogTool::EncodeString(kXzStreamHeaderMagicBytes,
                                  LogTool::Encoding::kBase64));

  EXPECT_EQ(kXzStreamHeaderMagicBytes,
            LogTool::EncodeString(kXzStreamHeaderMagicBytes,
                                  LogTool::Encoding::kBinary));
}

class LogTest : public testing::Test {
 protected:
  void SetUp() override {
    std::vector<char> buf(1024);

    uid_t uid = getuid();
    struct passwd pw_entry;
    struct passwd* pw_result;
    ASSERT_EQ(getpwuid_r(uid, &pw_entry, &buf[0], buf.size(), &pw_result), 0);
    ASSERT_NE(pw_result, nullptr);
    user_name_ = pw_entry.pw_name;

    gid_t gid = getgid();
    struct group gr_entry;
    struct group* gr_result;
    ASSERT_EQ(getgrgid_r(gid, &gr_entry, &buf[0], buf.size(), &gr_result), 0);
    ASSERT_NE(gr_result, nullptr);
    group_name_ = gr_entry.gr_name;
  }

  std::string user_name_;
  std::string group_name_;
};

TEST_F(LogTest, GetFileLogData) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  base::FilePath file_one = temp.GetPath().Append("test/file_one");
  ASSERT_TRUE(WriteFile(file_one, "test_one_contents"));
  const LogTool::Log log_one(LogTool::Log::kFile, "test_log_one",
                             file_one.value(), user_name_, group_name_);
  EXPECT_EQ(log_one.GetLogData(), "test_one_contents");

  base::FilePath file_two = temp.GetPath().Append("test/file_two");
  ASSERT_TRUE(WriteFile(file_two, ""));
  const LogTool::Log log_two(LogTool::Log::kFile, "test_log_two",
                             file_two.value(), user_name_, group_name_);
  EXPECT_EQ(log_two.GetLogData(), "<empty>");

  base::FilePath file_three = temp.GetPath().Append("test/file_three");
  ASSERT_TRUE(WriteFile(file_three, "long input value"));
  const LogTool::Log log_three(LogTool::Log::kFile, "test_log_three",
                               file_three.value(), user_name_, group_name_, 5);
  EXPECT_EQ(log_three.GetLogData(), "value");
}

TEST_F(LogTest, GetCommandLogData) {
  LogTool::Log log_one(LogTool::Log::kCommand, "test_log_one", "printf ''",
                       user_name_, group_name_);
  log_one.DisableMinijailForTest();
  EXPECT_EQ(log_one.GetLogData(), "<empty>");

  LogTool::Log log_two(LogTool::Log::kCommand, "test_log_two",
                       "printf 'test_output'", user_name_, group_name_);
  log_two.DisableMinijailForTest();
  EXPECT_EQ(log_two.GetLogData(), "test_output");

  LogTool::Log log_three(LogTool::Log::kCommand, "test_log_three",
                         "echo a,b,c | cut -d, -f2", user_name_,
                         group_name_);
  log_three.DisableMinijailForTest();
  EXPECT_EQ(log_three.GetLogData(), "b\n");
}
}  // namespace debugd
