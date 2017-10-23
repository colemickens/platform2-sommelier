// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crash-reporter/bert_collector.h"

#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <brillo/syslog_logging.h>
#include <fcntl.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "crash-reporter/test_util.h"

using base::FilePath;
using brillo::FindLog;

namespace {

constexpr char kACPITableDirectory[] = "sys/firmware/acpi/tables";

int s_crashes = 0;

void CountCrash() {
  ++s_crashes;
}

bool s_consent_given = true;

bool IsMetrics() {
  return s_consent_given;
}

}  // namespace

class BERTCollectorMock : public BERTCollector {
 public:
  MOCK_METHOD0(SetUpDBus, void());
};

class BERTCollectorTest : public ::testing::Test {
 protected:
  BERTCollectorMock collector_;
  FilePath test_dir_;
  base::ScopedTempDir scoped_temp_dir_;

  void PrepareBertDataTest(bool good_data) {
    constexpr char data[] = "Create BERT File for testing";
    FilePath testberttable_path = collector_.acpitable_path_.Append("BERT");
    FilePath testbertdata_path =
        collector_.acpitable_path_.Append("data/BERT");

    ASSERT_TRUE(test_util::CreateFile(testbertdata_path, data));

    if (!good_data) {
      ASSERT_TRUE(test_util::CreateFile(testberttable_path, data));
    } else {
      // Dummy test values.
      const struct acpi_table_bert bert_tab_test = {
        {'B', 'E', 'R', 'T'}, 48, 'A', 'D',
        "OEMID", "TABLEID", 0xFFFFFFFF, "ACP",
        0xEEEEEEEE, sizeof(data), 0x000000000001234
      };
      ASSERT_EQ(sizeof(struct acpi_table_bert),
                base::WriteFile(testberttable_path,
                                reinterpret_cast<const char*>(&bert_tab_test),
                                sizeof(struct acpi_table_bert)));
    }
  }

 public:
  void SetUp() override {
    s_consent_given = true;

    EXPECT_CALL(collector_, SetUpDBus()).WillRepeatedly(testing::Return());

    collector_.Initialize(CountCrash, IsMetrics);
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
    FilePath test_dir_ = scoped_temp_dir_.GetPath();

    collector_.set_crash_directory_for_test(test_dir_);
    collector_.acpitable_path_ = test_dir_.Append(kACPITableDirectory);
  }
};

TEST_F(BERTCollectorTest, TestNoBERTData) {
  ASSERT_FALSE(collector_.Collect());
  ASSERT_EQ(0, s_crashes);
}

TEST_F(BERTCollectorTest, TestNoConsent) {
  s_consent_given = false;
  PrepareBertDataTest(false);
  ASSERT_TRUE(collector_.Collect());
  ASSERT_TRUE(FindLog("(ignoring - no consent)"));
  ASSERT_EQ(0, s_crashes);
}

TEST_F(BERTCollectorTest, TestBadBERTData) {
  PrepareBertDataTest(false);
  ASSERT_FALSE(collector_.Collect());
  ASSERT_TRUE(FindLog("(handling)"));
  ASSERT_TRUE(FindLog("Bad data in BERT table"));
  ASSERT_EQ(0, s_crashes);
}

TEST_F(BERTCollectorTest, TestGoodBERTData) {
  PrepareBertDataTest(true);
  ASSERT_TRUE(collector_.Collect());
  ASSERT_TRUE(FindLog("(handling)"));
  ASSERT_TRUE(FindLog("Stored BERT dump"));
  ASSERT_EQ(1, s_crashes);
}
