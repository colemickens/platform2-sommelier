// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <linux/cec-funcs.h>

#include <memory>
#include <utility>
#include <vector>

#include <base/macros.h>
#include <gmock/gmock.h>

#include "cecservice/cec_device_mock.h"
#include "cecservice/cec_fd_mock.h"
#include "cecservice/cec_manager.h"
#include "cecservice/udev_mock.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SetArgPointee;

namespace cecservice {

class CecManagerTest : public ::testing::Test {
 public:
  CecManagerTest();
  ~CecManagerTest() = default;

 protected:
  CecDeviceFactoryMock cec_factory_mock_;
  Udev::DeviceCallback device_added_callback_;
  Udev::DeviceCallback device_removed_callback_;
  std::unique_ptr<UdevMock> udev_mock_ = std::make_unique<UdevMock>();
  NiceMock<UdevFactoryMock> udev_factory_mock_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CecManagerTest);
};

CecManagerTest::CecManagerTest() {
  ON_CALL(udev_factory_mock_, Create(_, _))
      .WillByDefault(
          Invoke([&](const Udev::DeviceCallback& device_added_callback,
                     const Udev::DeviceCallback& device_removed_callback) {
            device_added_callback_ = device_added_callback;
            device_removed_callback_ = device_removed_callback;

            return std::move(udev_mock_);
          }));
}

TEST_F(CecManagerTest, TestEnumerateAndCreate) {
  std::vector<base::FilePath> devices = {base::FilePath("/dev/cec0"),
                                         base::FilePath("/dev/cec1")};
  EXPECT_CALL(*udev_mock_, EnumerateDevices(_))
      .WillOnce(DoAll(SetArgPointee<0>(devices), Return(true)));

  EXPECT_CALL(cec_factory_mock_, Create(base::FilePath("/dev/cec0")))
      .WillOnce(Invoke([](const base::FilePath&) {
        return std::make_unique<CecDeviceMock>();
      }));
  EXPECT_CALL(cec_factory_mock_, Create(base::FilePath("/dev/cec1")))
      .WillOnce(Invoke([](const base::FilePath&) {
        return std::make_unique<CecDeviceMock>();
      }));

  auto cec_manager =
      std::make_unique<CecManager>(udev_factory_mock_, cec_factory_mock_);
}

TEST_F(CecManagerTest, TestAddRemoveDevice) {
  EXPECT_CALL(*udev_mock_, EnumerateDevices(_)).WillOnce(Return(true));
  auto cec_manager =
      std::make_unique<CecManager>(udev_factory_mock_, cec_factory_mock_);

  // Test device add.
  CecDeviceMock* device_mock;
  EXPECT_CALL(cec_factory_mock_, Create(base::FilePath("/dev/cec0")))
      .WillOnce(Invoke([&](const base::FilePath&) {
        auto mock = std::make_unique<CecDeviceMock>();
        device_mock = mock.get();
        return mock;
      }));
  device_added_callback_.Run(base::FilePath("/dev/cec0"));

  // Test removal.
  EXPECT_CALL(*device_mock, DestructorCalled());
  // Remove device.
  device_removed_callback_.Run(base::FilePath("/dev/cec0"));
  // Make sure that the device is now destroyed.
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(device_mock));
}

TEST_F(CecManagerTest, TestCommandForwarding) {
  std::vector<base::FilePath> devices{base::FilePath("/dev/cec0")};
  EXPECT_CALL(*udev_mock_, EnumerateDevices(_))
      .WillOnce(DoAll(SetArgPointee<0>(devices), Return(true)));

  CecDeviceMock* device_mock;
  EXPECT_CALL(cec_factory_mock_, Create(base::FilePath("/dev/cec0")))
      .WillOnce(Invoke([&](const base::FilePath&) {
        auto mock = std::make_unique<CecDeviceMock>();
        device_mock = mock.get();
        return mock;
      }));

  auto cec_manager =
      std::make_unique<CecManager>(udev_factory_mock_, cec_factory_mock_);

  EXPECT_CALL(*device_mock, SetStandBy());
  cec_manager->SetStandBy();

  EXPECT_CALL(*device_mock, SetWakeUp());
  cec_manager->SetWakeUp();
}

}  // namespace cecservice
