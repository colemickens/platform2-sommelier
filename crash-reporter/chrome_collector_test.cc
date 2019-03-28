// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crash-reporter/chrome_collector.h"

#include <stdio.h>

#include <base/auto_reset.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <brillo/syslog_logging.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "crash-reporter/test_util.h"

using base::FilePath;

namespace {

const char kCrashFormatGood[] = "value1:10:abcdefghijvalue2:5:12345";
const char kCrashFormatEmbeddedNewline[] =
    "value1:10:abcd\r\nghijvalue2:5:12\n34";
const char* const kCrashFormatBadValues[] = {
    "value1:10:abcdefghijvalue2:6=12345",
    "value1:10:abcdefghijvalue2:512345",
    "value1:10::abcdefghijvalue2:5=12345",
    "value1:10:abcdefghijvalue2:4=12345",
};

const char kCrashFormatWithFile[] =
    "value1:10:abcdefghijvalue2:5:12345"
    "some_file\"; filename=\"foo.txt\":15:12345\n789\n12345"
    "value3:2:ok";

bool s_allow_crash = false;

bool IsMetrics() {
  return s_allow_crash;
}

}  // namespace

class ChromeCollectorMock : public ChromeCollector {
 public:
  MOCK_METHOD0(SetUpDBus, void());
};

class ChromeCollectorTest : public ::testing::Test {
 protected:
  void ExpectFileEquals(const char* golden, const FilePath& file_path) {
    std::string contents;
    EXPECT_TRUE(base::ReadFileToString(file_path, &contents));
    EXPECT_EQ(golden, contents);
  }

  ChromeCollectorMock collector_;

 private:
  void SetUp() override {
    EXPECT_CALL(collector_, SetUpDBus()).WillRepeatedly(testing::Return());

    collector_.Initialize(IsMetrics, false);
    brillo::ClearLog();
  }
};

TEST_F(ChromeCollectorTest, GoodValues) {
  FilePath dir(".");
  EXPECT_TRUE(collector_.ParseCrashLog(kCrashFormatGood, dir,
                                       dir.Append("minidump.dmp"), "base"));

  // Check to see if the values made it in properly.
  std::string meta = collector_.extra_metadata_;
  EXPECT_TRUE(meta.find("value1=abcdefghij") != std::string::npos);
  EXPECT_TRUE(meta.find("value2=12345") != std::string::npos);
}

TEST_F(ChromeCollectorTest, Newlines) {
  FilePath dir(".");
  EXPECT_TRUE(collector_.ParseCrashLog(kCrashFormatEmbeddedNewline, dir,
                                       dir.Append("minidump.dmp"), "base"));

  // Check to see if the values were escaped.
  std::string meta = collector_.extra_metadata_;
  EXPECT_TRUE(meta.find("value1=abcd\\r\\nghij") != std::string::npos);
  EXPECT_TRUE(meta.find("value2=12\\n34") != std::string::npos);
}

TEST_F(ChromeCollectorTest, BadValues) {
  FilePath dir(".");
  for (const char* data : kCrashFormatBadValues) {
    brillo::ClearLog();
    EXPECT_FALSE(collector_.ParseCrashLog(data, dir, dir.Append("minidump.dmp"),
                                          "base"));
  }
}

TEST_F(ChromeCollectorTest, File) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const FilePath& dir = scoped_temp_dir.GetPath();
  EXPECT_TRUE(collector_.ParseCrashLog(kCrashFormatWithFile, dir,
                                       dir.Append("minidump.dmp"), "base"));

  // Check to see if the values are still correct and that the file was
  // written with the right data.
  std::string meta = collector_.extra_metadata_;
  EXPECT_TRUE(meta.find("value1=abcdefghij") != std::string::npos);
  EXPECT_TRUE(meta.find("value2=12345") != std::string::npos);
  EXPECT_TRUE(meta.find("value3=ok") != std::string::npos);
  ExpectFileEquals("12345\n789\n12345", dir.Append("base-foo.txt.other"));
}

TEST_F(ChromeCollectorTest, HandleCrash) {
  base::AutoReset<bool> auto_reset(&s_allow_crash, true);
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const FilePath& dir = scoped_temp_dir.GetPath();
  FilePath dump_file = dir.Append("test.dmp");
  ASSERT_TRUE(test_util::CreateFile(dump_file, kCrashFormatWithFile));
  collector_.set_crash_directory_for_test(dir);

  FilePath log_file;
  {
    base::ScopedFILE output(
        base::CreateAndOpenTemporaryFileInDir(dir, &log_file));
    ASSERT_TRUE(output.get());
    base::AutoReset<FILE*> auto_reset_file_ptr(&collector_.output_file_ptr_,
                                               output.get());
    EXPECT_TRUE(collector_.HandleCrash(dump_file, 123, 456, "chrome_test"));
  }
  ExpectFileEquals(ChromeCollector::kSuccessMagic, log_file);
}
