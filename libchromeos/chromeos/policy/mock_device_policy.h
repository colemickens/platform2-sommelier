// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOCK_DEVICE_POLICY_H_
#define MOCK_DEVICE_POLICY_H_

#include <string>
#include <vector>

#include <gmock/gmock.h>

#include "device_policy.h"

namespace policy {

// This is a generic mock class for the DevicePolicy that can be used by other
// subsystems for tests. It allows to mock out the reading of a real policy
// file and to simulate different policy values.
// The test that needs policies would then do something like this :
// // Prepare the action that would return a predefined policy value:
// ACTION_P(SetMetricsPolicy, enabled) {
//   *arg0 = enabled;
//   return true;
// }
// ...
// // Initialize the Mock class
// policy::MockDevicePolicy* device_policy = new policy::MockDevicePolicy();
// // We should expect calls to LoadPolicy almost always and return true.
// EXPECT_CALL(*device_policy_, LoadPolicy())
//     .Times(AnyNumber())
//     .WillRepeatedly(Return(true));
// // This example needs to simulate the Metrics Enabled policy being set.
// EXPECT_CALL(*device_policy_, GetMetricsEnabled(_))
//     .Times(AnyNumber())
//     .WillRepeatedly(SetMetricsPolicy(true));
// policy::PolicyProvider provider(device_policy);
// ...
// // In a test that needs other value of that policy we can do that:
// EXPECT_CALL(*device_policy_, GetMetricsEnabled(_))
//     .WillOnce(SetMetricsPolicy(false));
//
// See metrics_library_test.cc for example.
class MockDevicePolicy : public DevicePolicy {
 public:
  MOCK_METHOD0(LoadPolicy, bool(void));

  MOCK_CONST_METHOD1(GetPolicyRefreshRate, bool(int*));
  MOCK_CONST_METHOD1(GetUserWhitelist, bool(std::vector<std::string>*));
  MOCK_CONST_METHOD1(GetGuestModeEnabled, bool(bool*));
  MOCK_CONST_METHOD1(GetCameraEnabled, bool(bool*));
  MOCK_CONST_METHOD1(GetShowUserNames, bool(bool*));
  MOCK_CONST_METHOD1(GetDataRoamingEnabled, bool(bool*));
  MOCK_CONST_METHOD1(GetAllowNewUsers, bool(bool*));
  MOCK_CONST_METHOD1(GetMetricsEnabled, bool(bool*));
  MOCK_CONST_METHOD1(GetProxyMode, bool(std::string*));
  MOCK_CONST_METHOD1(GetProxyServer, bool(std::string*));
  MOCK_CONST_METHOD1(GetProxyPacUrl, bool(std::string*));
  MOCK_CONST_METHOD1(GetProxyBypassList, bool(std::string*));

  MOCK_METHOD0(VerifyPolicyFiles, bool(void));
  MOCK_METHOD0(VerifyPolicySignature, bool(void));
};
}  // namespace policy

#endif  // LIBPOLICY_DEVICE_POLICY_H_
