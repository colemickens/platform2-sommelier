// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/wakeup_device.h"

#include <limits>
#include <memory>
#include <string>

#include <base/strings/string_number_conversions.h>
#include <gtest/gtest.h>

#include "power_manager/common/power_constants.h"
#include "power_manager/powerd/system/udev_stub.h"

namespace power_manager {
namespace system {
namespace {
const char kTestSysfsPath[] = "/sys/devices/pci0000:00/0000:00:14.0/usb1/1-2/";
}
class WakeupDeviceTest : public testing::Test {
 public:
  WakeupDeviceTest()
      : wakeup_device_(std::make_unique<WakeupDevice>(
            base::FilePath(kTestSysfsPath), &udev_)) {}
  ~WakeupDeviceTest() override {}

 protected:
  UdevStub udev_;
  std::unique_ptr<WakeupDevice> wakeup_device_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WakeupDeviceTest);
};

// An incremented wakeup_count value should result in proper identification of
// wakeup device.
TEST_F(WakeupDeviceTest, TestWakeupCountIncrement) {
  const std::string kWakeupCountBeforeSuspendStr = "1";
  const uint64_t kWakeupCountBeforeSuspendInt = 1;
  udev_.SetSysattr(kTestSysfsPath, WakeupDevice::kPowerWakeupCount,
                   kWakeupCountBeforeSuspendStr);
  wakeup_device_->PrepareForSuspend();
  EXPECT_TRUE(wakeup_device_->was_pre_suspend_read_successful());
  EXPECT_EQ(wakeup_device_->wakeup_count_before_suspend(),
            kWakeupCountBeforeSuspendInt);

  const std::string kWakeupCountAfterResumeStr = "2";
  udev_.SetSysattr(kTestSysfsPath, WakeupDevice::kPowerWakeupCount,
                   kWakeupCountAfterResumeStr);
  wakeup_device_->HandleResume();
  EXPECT_TRUE(wakeup_device_->CausedLastWake());
}

// A overflow of wakeup_count value should result in proper identification of
// wakeup device.
TEST_F(WakeupDeviceTest, TestWakeupCountOverflow) {
  const std::string kWakeupCountBeforeSuspendStr =
      base::NumberToString(std::numeric_limits<uint64_t>::max());
  const uint64_t kWakeupCountBeforeSuspendInt =
      std::numeric_limits<uint64_t>::max();
  udev_.SetSysattr(kTestSysfsPath, WakeupDevice::kPowerWakeupCount,
                   kWakeupCountBeforeSuspendStr);
  wakeup_device_->PrepareForSuspend();
  EXPECT_TRUE(wakeup_device_->was_pre_suspend_read_successful());
  EXPECT_EQ(wakeup_device_->wakeup_count_before_suspend(),
            kWakeupCountBeforeSuspendInt);

  const std::string kWakeupCountAfterResumeStr =
      base::NumberToString(std::numeric_limits<uint64_t>::max() + 1);
  udev_.SetSysattr(kTestSysfsPath, WakeupDevice::kPowerWakeupCount,
                   kWakeupCountAfterResumeStr);
  wakeup_device_->HandleResume();
  EXPECT_TRUE(wakeup_device_->CausedLastWake());
}

// A empty wakeup_count file should result in proper identification of
// wakeup device.
TEST_F(WakeupDeviceTest, TestEmptyWakeupCountFile) {
  const std::string kWakeupCountBeforeSuspendStr = "";
  const uint64_t kWakeupCountBeforeSuspendInt = 0;
  udev_.SetSysattr(kTestSysfsPath, WakeupDevice::kPowerWakeupCount,
                   kWakeupCountBeforeSuspendStr);
  wakeup_device_->PrepareForSuspend();
  EXPECT_TRUE(wakeup_device_->was_pre_suspend_read_successful());
  EXPECT_EQ(wakeup_device_->wakeup_count_before_suspend(),
            kWakeupCountBeforeSuspendInt);

  const std::string kWakeupCountAfterResumeStr = "2";
  udev_.SetSysattr(kTestSysfsPath, WakeupDevice::kPowerWakeupCount,
                   kWakeupCountAfterResumeStr);
  wakeup_device_->HandleResume();
  EXPECT_TRUE(wakeup_device_->CausedLastWake());
}

// Failure to read the wakeup count before suspend should not mark the device as
// wake source.
TEST_F(WakeupDeviceTest, TestWakeupCountReadFailBeforeSuspend) {
  wakeup_device_->PrepareForSuspend();
  EXPECT_EQ(wakeup_device_->wakeup_count_before_suspend(), 0);
  EXPECT_FALSE(wakeup_device_->was_pre_suspend_read_successful());
  const std::string kWakeupCountAfterResumeStr = base::NumberToString(1);
  udev_.SetSysattr(kTestSysfsPath, WakeupDevice::kPowerWakeupCount,
                   kWakeupCountAfterResumeStr);
  wakeup_device_->HandleResume();
  EXPECT_FALSE(wakeup_device_->CausedLastWake());
}

// Failure to read the wakeup count after resume should not mark the device as
// wake source.
TEST_F(WakeupDeviceTest, TestWakeupCountReadFailAfterResume) {
  const std::string kWakeupCountBeforeSuspendStr = "1";
  udev_.SetSysattr(kTestSysfsPath, WakeupDevice::kPowerWakeupCount,
                   kWakeupCountBeforeSuspendStr);
  wakeup_device_->PrepareForSuspend();
  EXPECT_EQ(wakeup_device_->wakeup_count_before_suspend(), 1);
  EXPECT_TRUE(wakeup_device_->was_pre_suspend_read_successful());
  udev_.RemoveSysattr(kTestSysfsPath, WakeupDevice::kPowerWakeupCount);
  wakeup_device_->HandleResume();
  EXPECT_FALSE(wakeup_device_->CausedLastWake());
}

}  // namespace system
}  // namespace power_manager
