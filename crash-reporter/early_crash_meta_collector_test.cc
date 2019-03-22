// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crash-reporter/early_crash_meta_collector.h"

#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>

#include "crash-reporter/test_util.h"

namespace {

constexpr char kTestCrashFileName[] = "test_crash";
constexpr char kTestCrashFileContents[] = "Not a real crash.";

}  // namespace

class EarlyCrashMetaCollectorTest : public testing::Test {
 private:
  void SetUp() override {
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
    dest_dir_ = scoped_temp_dir_.GetPath().Append("crash_dest");
    src_dir_ = scoped_temp_dir_.GetPath().Append("crash_src");
    CreateDirectory(dest_dir_);
    CreateDirectory(src_dir_);
    collector_.set_crash_directory_for_test(dest_dir_);
    collector_.source_directory_ = src_dir_;
  }

 protected:
  void ExpectConsent() {
    collector_.Initialize([]() { return true; });
  }

  void ExpectConsentNotFound() {
    collector_.Initialize([]() { return false; });
  }

  void ExpectCrashReportsParsed() {
    ASSERT_TRUE(test_util::CreateFile(src_dir_.Append(kTestCrashFileName),
                                      kTestCrashFileContents));
    EXPECT_TRUE(collector_.Collect());
    EXPECT_FALSE(base::PathExists(src_dir_));
  }

  base::ScopedTempDir scoped_temp_dir_;
  base::FilePath dest_dir_, src_dir_;
  EarlyCrashMetaCollector collector_;
};

TEST_F(EarlyCrashMetaCollectorTest, CollectOk) {
  ExpectConsent();

  ExpectCrashReportsParsed();

  std::string content;
  base::FilePath destination_crash_file = dest_dir_.Append(kTestCrashFileName);
  EXPECT_TRUE(base::PathExists(destination_crash_file));
  base::ReadFileToString(destination_crash_file, &content);
  EXPECT_STREQ(content.c_str(), kTestCrashFileContents);
}

TEST_F(EarlyCrashMetaCollectorTest, NoConsent) {
  ExpectConsentNotFound();

  ExpectCrashReportsParsed();

  std::string content;
  base::FilePath destination_crash_file = dest_dir_.Append(kTestCrashFileName);

  EXPECT_FALSE(base::PathExists(destination_crash_file));
}
