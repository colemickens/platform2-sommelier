// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/device_claimer.h"

#include <string>

#include <gtest/gtest.h>

#include "shill/error.h"
#include "shill/mock_control.h"
#include "shill/mock_device_info.h"
#include "shill/mock_service_watcher.h"

using std::string;
using ::testing::_;
using ::testing::Mock;
using ::testing::Return;

namespace shill {

const char kServiceName[] = "org.chromium.TestService";
const char kTestDevice1Name[] = "test_device1";
const char kTestDevice2Name[] = "test_device2";

class DeviceClaimerTest : public testing::Test {
 public:
  DeviceClaimerTest()
     : device_info_(nullptr, nullptr, nullptr, nullptr),
       device_claimer_(kServiceName, &device_info_, false) {}

 protected:
  MockDeviceInfo device_info_;
  DeviceClaimer device_claimer_;
};

TEST_F(DeviceClaimerTest, StartServiceWatcher) {
  // Start service watcher.
  MockControl control;
  EXPECT_CALL(control, CreateRPCServiceWatcher(_, _))
      .WillOnce(Return(new MockServiceWatcher()));
  EXPECT_TRUE(device_claimer_.StartServiceWatcher(&control, base::Closure()));
  Mock::VerifyAndClearExpectations(&control);

  // Start service watcher again, should fail since name watcher already
  // started.
  EXPECT_CALL(control, CreateRPCServiceWatcher(_, _)).Times(0);
  EXPECT_FALSE(device_claimer_.StartServiceWatcher(&control, base::Closure()));
  Mock::VerifyAndClearExpectations(&control);
}

TEST_F(DeviceClaimerTest, ClaimAndReleaseDevices) {
  // Should not have any device claimed initially.
  EXPECT_FALSE(device_claimer_.DevicesClaimed());

  // Claim device 1.
  Error error;
  EXPECT_CALL(device_info_, AddDeviceToBlackList(kTestDevice1Name)).Times(1);
  EXPECT_TRUE(device_claimer_.Claim(kTestDevice1Name, &error));
  EXPECT_EQ(Error::kSuccess, error.type());
  EXPECT_TRUE(device_claimer_.DevicesClaimed());
  Mock::VerifyAndClearExpectations(&device_info_);

  // Claim device 2.
  error.Reset();
  EXPECT_CALL(device_info_, AddDeviceToBlackList(kTestDevice2Name)).Times(1);
  EXPECT_TRUE(device_claimer_.Claim(kTestDevice2Name, &error));
  EXPECT_EQ(Error::kSuccess, error.type());
  EXPECT_TRUE(device_claimer_.DevicesClaimed());
  Mock::VerifyAndClearExpectations(&device_info_);

  // Claim device 1 again, should fail since it is already claimed.
  const char kDuplicateDevice1Error[] =
      "Device test_device1 had already been claimed";
  error.Reset();
  EXPECT_CALL(device_info_, AddDeviceToBlackList(_)).Times(0);
  EXPECT_FALSE(device_claimer_.Claim(kTestDevice1Name, &error));
  EXPECT_EQ(string(kDuplicateDevice1Error), error.message());
  Mock::VerifyAndClearExpectations(&device_info_);

  // Release device 1.
  error.Reset();
  EXPECT_CALL(device_info_,
              RemoveDeviceFromBlackList(kTestDevice1Name)).Times(1);
  EXPECT_TRUE(device_claimer_.Release(kTestDevice1Name, &error));
  EXPECT_EQ(Error::kSuccess, error.type());
  // Should still have one device claimed.
  EXPECT_TRUE(device_claimer_.DevicesClaimed());
  Mock::VerifyAndClearExpectations(&device_info_);

  // Release device 1 again, should fail since device 1 is not currently
  // claimed.
  const char kDevice1NotClaimedError[] =
      "Device test_device1 have not been claimed";
  error.Reset();
  EXPECT_CALL(device_info_, RemoveDeviceFromBlackList(_)).Times(0);
  EXPECT_FALSE(device_claimer_.Release(kTestDevice1Name, &error));
  EXPECT_EQ(string(kDevice1NotClaimedError), error.message());
  // Should still have one device claimed.
  EXPECT_TRUE(device_claimer_.DevicesClaimed());
  Mock::VerifyAndClearExpectations(&device_info_);

  // Release device 2
  error.Reset();
  EXPECT_CALL(device_info_,
              RemoveDeviceFromBlackList(kTestDevice2Name)).Times(1);
  EXPECT_TRUE(device_claimer_.Release(kTestDevice2Name, &error));
  EXPECT_EQ(Error::kSuccess, error.type());
  Mock::VerifyAndClearExpectations(&device_info_);

  // Should not have any claimed devices.
  EXPECT_FALSE(device_claimer_.DevicesClaimed());
}

}  // namespace shill
