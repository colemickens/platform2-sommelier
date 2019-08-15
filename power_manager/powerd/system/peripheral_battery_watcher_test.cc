// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/peripheral_battery_watcher.h"

#include <string>

#include <base/compiler_specific.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/strings/string_number_conversions.h>
#include <chromeos/dbus/service_constants.h>
#include <gtest/gtest.h>

#include "power_manager/common/test_main_loop_runner.h"
#include "power_manager/powerd/system/dbus_wrapper_stub.h"
#include "power_manager/proto_bindings/peripheral_battery_status.pb.h"

namespace power_manager {
namespace system {

using std::string;

namespace {

// Abort if it an expected battery update hasn't been received after this long.
constexpr base::TimeDelta kUpdateTimeout = base::TimeDelta::FromSeconds(3);

// Shorter update timeout to use when failure is expected.
constexpr base::TimeDelta kShortUpdateTimeout =
    base::TimeDelta::FromMilliseconds(100);

const char kDeviceModelName[] = "Test HID Mouse";

class TestWrapper : public DBusWrapperStub {
 public:
  TestWrapper() {}
  ~TestWrapper() override {}

  // Runs |loop_| until battery status is sent through D-Bus.
  bool RunUntilSignalSent(const base::TimeDelta& timeout) {
    return loop_runner_.StartLoop(timeout);
  }

  void EmitBareSignal(const std::string& signal_name) override {
    DBusWrapperStub::EmitBareSignal(signal_name);
    loop_runner_.StopLoop();
  }

  void EmitSignalWithProtocolBuffer(
      const std::string& signal_name,
      const google::protobuf::MessageLite& protobuf) override {
    DBusWrapperStub::EmitSignalWithProtocolBuffer(signal_name, protobuf);
    loop_runner_.StopLoop();
  }

 private:
  TestMainLoopRunner loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(TestWrapper);
};

}  // namespace

class PeripheralBatteryWatcherTest : public ::testing::Test {
 public:
  PeripheralBatteryWatcherTest() {}
  ~PeripheralBatteryWatcherTest() override {}

  void SetUp() override {
    CHECK(temp_dir_.CreateUniqueTempDir());
    base::FilePath device_dir = temp_dir_.GetPath().Append("hid-1-battery");
    CHECK(base::CreateDirectory(device_dir));
    scope_file_ = device_dir.Append(PeripheralBatteryWatcher::kScopeFile);
    WriteFile(scope_file_, PeripheralBatteryWatcher::kScopeValueDevice);
    status_file_ = device_dir.Append(PeripheralBatteryWatcher::kStatusFile);
    model_name_file_ =
        device_dir.Append(PeripheralBatteryWatcher::kModelNameFile);
    WriteFile(model_name_file_, kDeviceModelName);
    capacity_file_ = device_dir.Append(PeripheralBatteryWatcher::kCapacityFile);
    battery_.set_battery_path_for_testing(temp_dir_.GetPath());
  }

 protected:
  void WriteFile(const base::FilePath& path, const string& str) {
    ASSERT_EQ(str.size(), base::WriteFile(path, str.data(), str.size()));
  }

  // Temporary directory mimicking a /sys directory containing a set of sensor
  // devices.
  base::ScopedTempDir temp_dir_;

  base::FilePath scope_file_;
  base::FilePath status_file_;
  base::FilePath capacity_file_;
  base::FilePath model_name_file_;

  TestWrapper test_wrapper_;

  PeripheralBatteryWatcher battery_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PeripheralBatteryWatcherTest);
};

TEST_F(PeripheralBatteryWatcherTest, Basic) {
  std::string level = base::NumberToString(80);
  WriteFile(capacity_file_, level);
  battery_.Init(&test_wrapper_);
  ASSERT_TRUE(test_wrapper_.RunUntilSignalSent(kUpdateTimeout));

  EXPECT_EQ(1, test_wrapper_.num_sent_signals());
  PeripheralBatteryStatus proto;
  EXPECT_TRUE(test_wrapper_.GetSentSignal(0, kPeripheralBatteryStatusSignal,
                                          &proto, nullptr));
  EXPECT_EQ(80, proto.level());
  EXPECT_EQ(kDeviceModelName, proto.name());
}

TEST_F(PeripheralBatteryWatcherTest, NoLevelReading) {
  battery_.Init(&test_wrapper_);
  // Without writing battery level to the capacity_file_, the loop
  // will timeout.
  EXPECT_FALSE(test_wrapper_.RunUntilSignalSent(kShortUpdateTimeout));
}

TEST_F(PeripheralBatteryWatcherTest, SkipUnknownStatus) {
  // Batteries with unknown statuses should be skipped: http://b/64397082
  WriteFile(capacity_file_, base::NumberToString(0));
  WriteFile(status_file_, PeripheralBatteryWatcher::kStatusValueUnknown);
  battery_.Init(&test_wrapper_);
  ASSERT_FALSE(test_wrapper_.RunUntilSignalSent(kShortUpdateTimeout));
}

TEST_F(PeripheralBatteryWatcherTest, AllowOtherStatus) {
  // Batteries with other statuses should be reported.
  WriteFile(capacity_file_, base::NumberToString(20));
  WriteFile(status_file_, "Discharging");
  battery_.Init(&test_wrapper_);
  ASSERT_TRUE(test_wrapper_.RunUntilSignalSent(kUpdateTimeout));

  EXPECT_EQ(1, test_wrapper_.num_sent_signals());
  PeripheralBatteryStatus proto;
  EXPECT_TRUE(test_wrapper_.GetSentSignal(0, kPeripheralBatteryStatusSignal,
                                          &proto, nullptr));
  EXPECT_EQ(20, proto.level());
}

}  // namespace system
}  // namespace power_manager
