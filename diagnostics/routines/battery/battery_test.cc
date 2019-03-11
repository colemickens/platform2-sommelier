// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include <base/files/file_path.h>
#include <base/files/scoped_temp_dir.h>
#include <base/macros.h>
#include <gtest/gtest.h>

#include "diagnostics/routines/battery/battery.h"
#include "diagnostics/wilco_dtc_supportd/file_test_utils.h"
#include "wilco_dtc_supportd.pb.h"  // NOLINT(build/include)

namespace diagnostics {

namespace {
constexpr int kLowmAh = 1000;
constexpr int kHighmAh = 10000;
constexpr int kGoodFileContents = 8948000;
constexpr int kBadFileContents = 10;

std::string FakeGoodFileContents() {
  return std::to_string(kGoodFileContents);
}

std::string FakeBadFileContents() {
  return std::to_string(kBadFileContents);
}

}  // namespace

class BatteryRoutineTest : public testing::Test {
 protected:
  BatteryRoutineTest() {
    params_.set_low_mah(kLowmAh);
    params_.set_high_mah(kHighmAh);
    routine_ = std::make_unique<BatteryRoutine>(params_);
  }

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    routine_->set_root_dir_for_testing(temp_dir_.GetPath());
  }

  BatteryRoutine* routine() { return routine_.get(); }

  grpc_api::GetRoutineUpdateResponse* response() { return &response_; }

  void RunRoutineAndWaitForExit() {
    routine_->Start();

    // Since the BatteryRoutine has finished by the time Start() returns, there
    // is no need to wait.
    routine_->PopulateStatusUpdate(&response_, true);
  }

  void WriteChargeFullDesign(const std::string& file_contents) {
    EXPECT_TRUE(WriteFileAndCreateParentDirs(
        temp_dir_path().Append(base::FilePath(kBatteryChargeFullDesignPath)),
        file_contents));
  }

  const base::FilePath& temp_dir_path() const { return temp_dir_.GetPath(); }

 private:
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<BatteryRoutine> routine_;
  grpc_api::GetRoutineUpdateResponse response_;
  grpc_api::BatteryRoutineParameters params_;

  DISALLOW_COPY_AND_ASSIGN(BatteryRoutineTest);
};

// Test that the battery routine fails if charge_full_design does not exist.
TEST_F(BatteryRoutineTest, NoChargeFullDesign) {
  RunRoutineAndWaitForExit();
  EXPECT_EQ(response()->status_message(), kBatteryNoChargeFullDesignMessage);
  EXPECT_EQ(response()->status(), grpc_api::ROUTINE_STATUS_ERROR);
}

// Test that the battery routine fails if charge_full_design is outside the
// limits.
TEST_F(BatteryRoutineTest, LowChargeFullDesign) {
  WriteChargeFullDesign(FakeBadFileContents());
  RunRoutineAndWaitForExit();
  EXPECT_EQ(response()->status_message(), kBatteryRoutineFailedMessage);
  EXPECT_EQ(response()->status(), grpc_api::ROUTINE_STATUS_FAILED);
}

// Test that the battery routine passes if charge_full_design is within the
// limits.
TEST_F(BatteryRoutineTest, GoodChargeFullDesign) {
  WriteChargeFullDesign(FakeGoodFileContents());
  RunRoutineAndWaitForExit();
  EXPECT_EQ(response()->status_message(), kBatteryRoutineSucceededMessage);
  EXPECT_EQ(response()->status(), grpc_api::ROUTINE_STATUS_PASSED);
}

}  // namespace diagnostics
