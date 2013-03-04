// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "power_manager/common/test_main_loop_runner.h"
#include "power_manager/powerd/system/peripheral_battery_watcher.h"

namespace power_manager {
namespace system {

using std::string;

namespace {

// Abort if it an expected battery update hasn't been received after this
// many milliseconds.
const int kUpdateTimeoutMs = 1000;

const char kDeviceModelName[] = "Test HID Mouse";

// Simple PeripheralBatteryObserver implementation that runs the event loop
// until it receives battery level update.
class TestObserver : public PeripheralBatteryObserver {
 public:
  TestObserver() : level_(0) {}
  virtual ~TestObserver() {}

  // Runs |loop_| until OnPeripheralBatteryUpdate() is called.
  bool RunUntilBatteryUpdate() {
    return loop_runner_.StartLoop(
        base::TimeDelta::FromMilliseconds(kUpdateTimeoutMs));
  }

  virtual void OnPeripheralBatteryUpdate(const string& path,
                                         const string& model_name,
                                         int level) {
    model_name_ = model_name;
    level_ = level;
    LOG(INFO) << "Stopping loop after battery update";
    loop_runner_.StopLoop();
  }

  virtual void OnPeripheralBatteryError(const string& path,
                                        const string& model_name) {
  }

  int level_;
  string model_name_;

 private:
  TestMainLoopRunner loop_runner_;
  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

}  // namespace

class PeripheralBatteryWatcherTest : public ::testing::Test {
 public:
  PeripheralBatteryWatcherTest() {}
  virtual ~PeripheralBatteryWatcherTest() {}

  virtual void SetUp() OVERRIDE {
    CHECK(temp_dir_.CreateUniqueTempDir());
    base::FilePath device_dir = temp_dir_.path().Append("hid-1-battery");
    CHECK(file_util::CreateDirectory(device_dir));
    scope_file_ = device_dir.Append("scope");
    WriteFile(scope_file_, "Device");
    model_name_file_ = device_dir.Append("model_name");
    WriteFile(model_name_file_, kDeviceModelName);
    capacity_file_ = device_dir.Append("capacity");
    battery_.set_battery_path_for_testing(temp_dir_.path());
    battery_.AddObserver(&observer_);
  }

  virtual void TearDown() OVERRIDE {
    LOG(INFO) << "Tearing down test";
    battery_.RemoveObserver(&observer_);
  }

 protected:
  void WriteFile(const base::FilePath& path, const string& str) {
    int bytes_written =
        file_util::WriteFile(path, str.data(), str.size());
    CHECK(bytes_written == static_cast<int>(str.size()))
        << "Wrote " << bytes_written << " byte(s) instead of "
        << str.size() << " to " << path.value();
  }

  // Temporary directory mimicking a /sys directory containing a set of sensor
  // devices.
  base::ScopedTempDir temp_dir_;

  base::FilePath scope_file_;
  base::FilePath capacity_file_;
  base::FilePath model_name_file_;

  TestObserver observer_;

  PeripheralBatteryWatcher battery_;

  DISALLOW_COPY_AND_ASSIGN(PeripheralBatteryWatcherTest);
};

TEST_F(PeripheralBatteryWatcherTest, Basic) {
  std::string level = base::IntToString(80);
  WriteFile(capacity_file_, level);
  battery_.Init();
  ASSERT_TRUE(observer_.RunUntilBatteryUpdate());
  EXPECT_EQ(80, observer_.level_);
  EXPECT_EQ(kDeviceModelName, observer_.model_name_);
}

TEST_F(PeripheralBatteryWatcherTest, NoLevelReading) {
  battery_.Init();
  //  Without writing battery level to the capacity_file_, the loop
  //  will timeout.
  ASSERT_FALSE(observer_.RunUntilBatteryUpdate());
}

}  // namespace system
}  // namespace power_manager
