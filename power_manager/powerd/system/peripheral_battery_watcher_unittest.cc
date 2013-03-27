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
#include "chromeos/dbus/service_constants.h"
#include "power_manager/common/dbus_sender_stub.h"
#include "power_manager/common/test_main_loop_runner.h"
#include "power_manager/peripheral_battery_status.pb.h"
#include "power_manager/powerd/system/peripheral_battery_watcher.h"

namespace power_manager {
namespace system {

using std::string;

namespace {

// Abort if it an expected battery update hasn't been received after this
// many milliseconds.
const int kUpdateTimeoutMs = 1000;

const char kDeviceModelName[] = "Test HID Mouse";

class TestSender : public DBusSenderStub {
 public:
  TestSender() {}
  virtual ~TestSender() {}

  // Runs |loop_| until battery status is sent through D-Bus.
  bool RunUntilSignalSent() {
    return loop_runner_.StartLoop(
        base::TimeDelta::FromMilliseconds(kUpdateTimeoutMs));
  }

  virtual void EmitBareSignal(const std::string& signal_name) {
    DBusSenderStub::EmitBareSignal(signal_name);
    loop_runner_.StopLoop();
  }

  virtual void EmitSignalWithProtocolBuffer(const std::string& signal_name,
    const google::protobuf::MessageLite& protobuf) {
    DBusSenderStub::EmitSignalWithProtocolBuffer(signal_name, protobuf);
    loop_runner_.StopLoop();
  }

 private:
  TestMainLoopRunner loop_runner_;
  DISALLOW_COPY_AND_ASSIGN(TestSender);
};

}  // namespace

class PeripheralBatteryWatcherTest : public ::testing::Test {
 public:
  PeripheralBatteryWatcherTest() : battery_(&test_sender_) {}
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

  TestSender test_sender_;

  PeripheralBatteryWatcher battery_;

  DISALLOW_COPY_AND_ASSIGN(PeripheralBatteryWatcherTest);
};

TEST_F(PeripheralBatteryWatcherTest, Basic) {
  std::string level = base::IntToString(80);
  WriteFile(capacity_file_, level);
  battery_.Init();
  ASSERT_TRUE(test_sender_.RunUntilSignalSent());
  EXPECT_EQ(1, test_sender_.num_sent_signals());
  PeripheralBatteryStatus proto;
  EXPECT_TRUE(test_sender_.GetSentSignal(0,
                                         kPeripheralBatteryStatusSignal,
                                         &proto));
  EXPECT_EQ(80, proto.level());
  EXPECT_EQ(kDeviceModelName, proto.name());
}

TEST_F(PeripheralBatteryWatcherTest, NoLevelReading) {
  battery_.Init();
  //  Without writing battery level to the capacity_file_, the loop
  //  will timeout.
  ASSERT_FALSE(test_sender_.RunUntilSignalSent());
}

}  // namespace system
}  // namespace power_manager
