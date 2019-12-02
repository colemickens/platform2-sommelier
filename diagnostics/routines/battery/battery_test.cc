// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include <base/files/file_path.h>
#include <base/files/scoped_temp_dir.h>
#include <base/macros.h>
#include <gtest/gtest.h>

#include "diagnostics/common/file_test_utils.h"
#include "diagnostics/routines/battery/battery.h"
#include "mojo/cros_healthd_diagnostics.mojom.h"

namespace diagnostics {
namespace mojo_ipc = ::chromeos::cros_healthd::mojom;

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
    routine_ = std::make_unique<BatteryRoutine>(kLowmAh, kHighmAh);
  }

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    routine_->set_root_dir_for_testing(temp_dir_.GetPath());
  }

  BatteryRoutine* routine() { return routine_.get(); }

  mojo_ipc::RoutineUpdate* update() { return &update_; }

  void RunRoutineAndWaitForExit() {
    routine_->Start();

    // Since the BatteryRoutine has finished by the time Start() returns, there
    // is no need to wait.
    routine_->PopulateStatusUpdate(&update_, true);
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
  mojo_ipc::RoutineUpdate update_{0, mojo::ScopedHandle(),
                                  mojo_ipc::RoutineUpdateUnion::New()};

  DISALLOW_COPY_AND_ASSIGN(BatteryRoutineTest);
};

// Test that the battery routine fails if charge_full_design does not exist.
TEST_F(BatteryRoutineTest, NoChargeFullDesign) {
  RunRoutineAndWaitForExit();
  const auto& update_union = update()->routine_update_union;
  ASSERT_FALSE(update_union.is_null());
  ASSERT_TRUE(update_union->is_noninteractive_update());
  const auto& noninteractive_update = update_union->get_noninteractive_update();
  EXPECT_EQ(noninteractive_update->status_message,
            kBatteryNoChargeFullDesignMessage);
  EXPECT_EQ(noninteractive_update->status,
            mojo_ipc::DiagnosticRoutineStatusEnum::kError);
}

// Test that the battery routine fails if charge_full_design is outside the
// limits.
TEST_F(BatteryRoutineTest, LowChargeFullDesign) {
  WriteChargeFullDesign(FakeBadFileContents());
  RunRoutineAndWaitForExit();
  const auto& update_union = update()->routine_update_union;
  ASSERT_FALSE(update_union.is_null());
  ASSERT_TRUE(update_union->is_noninteractive_update());
  const auto& noninteractive_update = update_union->get_noninteractive_update();
  EXPECT_EQ(noninteractive_update->status_message,
            kBatteryRoutineFailedMessage);
  EXPECT_EQ(noninteractive_update->status,
            mojo_ipc::DiagnosticRoutineStatusEnum::kFailed);
}

// Test that the battery routine passes if charge_full_design is within the
// limits.
TEST_F(BatteryRoutineTest, GoodChargeFullDesign) {
  WriteChargeFullDesign(FakeGoodFileContents());
  RunRoutineAndWaitForExit();
  const auto& update_union = update()->routine_update_union;
  ASSERT_FALSE(update_union.is_null());
  ASSERT_TRUE(update_union->is_noninteractive_update());
  const auto& noninteractive_update = update_union->get_noninteractive_update();
  EXPECT_EQ(noninteractive_update->status_message,
            kBatteryRoutineSucceededMessage);
  EXPECT_EQ(noninteractive_update->status,
            mojo_ipc::DiagnosticRoutineStatusEnum::kPassed);
}

}  // namespace diagnostics
