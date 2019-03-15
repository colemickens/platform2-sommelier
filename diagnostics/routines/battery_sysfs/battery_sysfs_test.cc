// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include <base/files/file_path.h>
#include <base/files/scoped_temp_dir.h>
#include <base/macros.h>
#include <base/strings/string_split.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "diagnostics/routines/battery_sysfs/battery_sysfs.h"
#include "diagnostics/wilco_dtc_supportd/file_test_utils.h"
#include "wilco_dtc_supportd.pb.h"  // NOLINT(build/include)

namespace diagnostics {

namespace {

using ::testing::UnorderedElementsAreArray;

constexpr int kMaximumCycleCount = 5;
constexpr int kPercentBatteryWearAllowed = 10;
constexpr int kHighCycleCount = 6;
constexpr int kLowCycleCount = 4;
constexpr int kHighChargeFull = 91;
constexpr int kLowChargeFull = 89;
constexpr int kFakeBatteryChargeFullDesign = 100;
constexpr char kFakeManufacturer[] = "Fake Manufacturer";
constexpr int kFakeCurrentNow = 90871023;
constexpr int kFakePresent = 1;
constexpr char kFakeStatus[] = "Full";
constexpr int kFakeVoltageNow = 90872;
constexpr int kFakeChargeNow = 98123;
constexpr char kFullCycleCountPath[] =
    "sys/class/power_supply/BAT0/cycle_count";

std::string ConstructOutput() {
  std::string output;
  int wear_percentage =
      100 - (kHighChargeFull * 100 / kFakeBatteryChargeFullDesign);
  output += "Wear Percentage: " + std::to_string(wear_percentage) + "\n";
  output += "Cycle Count: " + std::to_string(kLowCycleCount) + "\n";
  output += "Manufacturer: " + std::string(kFakeManufacturer) + "\n";
  output += "Current Now: " + std::to_string(kFakeCurrentNow) + "\n";
  output += "Present: " + std::to_string(kFakePresent) + "\n";
  output += "Status: " + std::string(kFakeStatus) + "\n";
  output += "Voltage Now: " + std::to_string(kFakeVoltageNow) + "\n";
  output += "Charge Full: " + std::to_string(kHighChargeFull) + "\n";
  output +=
      "Charge Full Design: " + std::to_string(kFakeBatteryChargeFullDesign) +
      "\n";
  output += "Charge Now: " + std::to_string(kFakeChargeNow) + "\n";
  return output;
}

}  // namespace

class BatterySysfsRoutineTest : public testing::Test {
 protected:
  BatterySysfsRoutineTest() {
    params_.set_maximum_cycle_count(kMaximumCycleCount);
    params_.set_percent_battery_wear_allowed(kPercentBatteryWearAllowed);
    routine_ = std::make_unique<BatterySysfsRoutine>(params_);
  }

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    routine_->set_root_dir_for_testing(temp_dir_.GetPath());
  }

  BatterySysfsRoutine* routine() { return routine_.get(); }

  grpc_api::GetRoutineUpdateResponse* response() { return &response_; }

  void RunRoutineAndWaitForExit() {
    routine_->Start();

    // Since the BatterySysfsRoutine has finished by the time Start() returns,
    // there is no need to wait.
    routine_->PopulateStatusUpdate(&response_, true);
  }

  void WriteFilesReadByLog() {
    WriteFileContents(kBatterySysfsManufacturerPath, kFakeManufacturer);
    WriteFileContents(kBatterySysfsCurrentNowPath,
                      std::to_string(kFakeCurrentNow));
    WriteFileContents(kBatterySysfsPresentPath, std::to_string(kFakePresent));
    WriteFileContents(kBatterySysfsStatusPath, kFakeStatus);
    WriteFileContents(kBatterySysfsVoltageNowPath,
                      std::to_string(kFakeVoltageNow));
    WriteFileContents(kBatterySysfsChargeNowPath,
                      std::to_string(kFakeChargeNow));
  }

  void WriteFileContents(const std::string& relative_file_path,
                         const std::string& file_contents) {
    EXPECT_TRUE(
        WriteFileAndCreateParentDirs(temp_dir_path()
                                         .AppendASCII(kBatterySysfsPath)
                                         .AppendASCII(relative_file_path),
                                     file_contents));
  }

  const base::FilePath& temp_dir_path() const { return temp_dir_.GetPath(); }

 private:
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<BatterySysfsRoutine> routine_;
  grpc_api::GetRoutineUpdateResponse response_;
  grpc_api::BatterySysfsRoutineParameters params_;

  DISALLOW_COPY_AND_ASSIGN(BatterySysfsRoutineTest);
};

// Test that the battery_sysfs routine fails if the cycle count is too high.
TEST_F(BatterySysfsRoutineTest, HighCycleCount) {
  WriteFileContents(kBatterySysfsChargeFullPath,
                    std::to_string(kHighChargeFull));
  WriteFileContents(kBatterySysfsChargeFullDesignPath,
                    std::to_string(kFakeBatteryChargeFullDesign));
  WriteFileContents(kBatterySysfsCycleCountPath,
                    std::to_string(kHighCycleCount));
  RunRoutineAndWaitForExit();
  EXPECT_EQ(response()->status_message(),
            kBatterySysfsExcessiveCycleCountMessage);
  EXPECT_EQ(response()->status(), grpc_api::ROUTINE_STATUS_FAILED);
}

// Test that the battery_sysfs routine fails if cycle_count is not present.
TEST_F(BatterySysfsRoutineTest, NoCycleCount) {
  WriteFileContents(kBatterySysfsChargeFullPath,
                    std::to_string(kHighChargeFull));
  WriteFileContents(kBatterySysfsChargeFullDesignPath,
                    std::to_string(kFakeBatteryChargeFullDesign));
  RunRoutineAndWaitForExit();
  EXPECT_EQ(response()->status_message(),
            kBatterySysfsFailedReadingCycleCountMessage);
  EXPECT_EQ(response()->status(), grpc_api::ROUTINE_STATUS_ERROR);
}

// Test that the battery_sysfs routine fails if the wear percentage is too
// high.
TEST_F(BatterySysfsRoutineTest, HighWearPercentage) {
  WriteFileContents(kBatterySysfsChargeFullPath,
                    std::to_string(kLowChargeFull));
  WriteFileContents(kBatterySysfsChargeFullDesignPath,
                    std::to_string(kFakeBatteryChargeFullDesign));
  WriteFileContents(kBatterySysfsCycleCountPath,
                    std::to_string(kLowCycleCount));
  RunRoutineAndWaitForExit();
  EXPECT_EQ(response()->status_message(), kBatterySysfsExcessiveWearMessage);
  EXPECT_EQ(response()->status(), grpc_api::ROUTINE_STATUS_FAILED);
}

// Test that the battery_sysfs routine fails if neither charge_full nor
// energy_full are present.
TEST_F(BatterySysfsRoutineTest, NoWearPercentage) {
  WriteFileContents(kBatterySysfsCycleCountPath,
                    std::to_string(kLowCycleCount));
  RunRoutineAndWaitForExit();
  EXPECT_EQ(response()->status_message(),
            kBatterySysfsFailedCalculatingWearPercentageMessage);
  EXPECT_EQ(response()->status(), grpc_api::ROUTINE_STATUS_ERROR);
}

// Test that the battery_sysfs routine passes if the cycle count and wear
// percentage are within acceptable limits.
TEST_F(BatterySysfsRoutineTest, GoodParameters) {
  WriteFileContents(kBatterySysfsChargeFullPath,
                    std::to_string(kHighChargeFull));
  WriteFileContents(kBatterySysfsChargeFullDesignPath,
                    std::to_string(kFakeBatteryChargeFullDesign));
  WriteFileContents(kBatterySysfsCycleCountPath,
                    std::to_string(kLowCycleCount));
  WriteFilesReadByLog();
  RunRoutineAndWaitForExit();
  EXPECT_EQ(response()->status_message(), kBatterySysfsRoutinePassedMessage);
  EXPECT_EQ(response()->status(), grpc_api::ROUTINE_STATUS_PASSED);
  base::StringPairs expected_output_pairs;
  base::StringPairs actual_output_pairs;
  ASSERT_TRUE(base::SplitStringIntoKeyValuePairs(ConstructOutput(), ':', '\n',
                                                 &expected_output_pairs));
  ASSERT_TRUE(base::SplitStringIntoKeyValuePairs(response()->output(), ':',
                                                 '\n', &actual_output_pairs));
  EXPECT_THAT(actual_output_pairs,
              UnorderedElementsAreArray(expected_output_pairs));
}

// Test that the battery_sysfs routine will find energy-reporting batteries.
TEST_F(BatterySysfsRoutineTest, EnergyReportingBattery) {
  WriteFileContents(kBatterySysfsEnergyFullPath,
                    std::to_string(kHighChargeFull));
  WriteFileContents(kBatterySysfsEnergyFullDesignPath,
                    std::to_string(kFakeBatteryChargeFullDesign));
  WriteFileContents(kBatterySysfsCycleCountPath,
                    std::to_string(kLowCycleCount));
  RunRoutineAndWaitForExit();
  EXPECT_EQ(response()->status_message(), kBatterySysfsRoutinePassedMessage);
  EXPECT_EQ(response()->status(), grpc_api::ROUTINE_STATUS_PASSED);
}

// Test that the battery_sysfs routine uses the expected full path to
// cycle_count, relative to the temporary test directory.
TEST_F(BatterySysfsRoutineTest, FullCycleCountPath) {
  WriteFileContents(kBatterySysfsChargeFullPath,
                    std::to_string(kHighChargeFull));
  WriteFileContents(kBatterySysfsChargeFullDesignPath,
                    std::to_string(kFakeBatteryChargeFullDesign));
  EXPECT_TRUE(WriteFileAndCreateParentDirs(
      temp_dir_path().AppendASCII(kFullCycleCountPath),
      std::to_string(kLowCycleCount)));
  RunRoutineAndWaitForExit();
  EXPECT_EQ(response()->status_message(), kBatterySysfsRoutinePassedMessage);
  EXPECT_EQ(response()->status(), grpc_api::ROUTINE_STATUS_PASSED);
}

}  // namespace diagnostics
