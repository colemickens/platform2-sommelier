// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "policy/device_policy_impl.h"

#include "bindings/chrome_device_policy.pb.h"

namespace em = enterprise_management;

namespace policy {

// Enterprise managed.
TEST(DevicePolicyImplTest, GetOwner_Managed) {
  em::PolicyData policy_data;
  policy_data.set_username("user@example.com");
  policy_data.set_management_mode(em::PolicyData::ENTERPRISE_MANAGED);
  DevicePolicyImpl device_policy;
  device_policy.set_policy_data_for_testing(policy_data);

  std::string owner("something");
  EXPECT_TRUE(device_policy.GetOwner(&owner));
  EXPECT_TRUE(owner.empty());
}

// Consumer owned.
TEST(DevicePolicyImplTest, GetOwner_Consumer) {
  em::PolicyData policy_data;
  policy_data.set_username("user@example.com");
  policy_data.set_management_mode(em::PolicyData::LOCAL_OWNER);
  policy_data.set_request_token("codepath-must-ignore-dmtoken");
  DevicePolicyImpl device_policy;
  device_policy.set_policy_data_for_testing(policy_data);

  std::string owner;
  EXPECT_TRUE(device_policy.GetOwner(&owner));
  EXPECT_EQ("user@example.com", owner);
}

// Consumer owned, username is missing.
TEST(DevicePolicyImplTest, GetOwner_ConsumerMissingUsername) {
  em::PolicyData policy_data;
  DevicePolicyImpl device_policy;
  device_policy.set_policy_data_for_testing(policy_data);

  std::string owner("something");
  EXPECT_FALSE(device_policy.GetOwner(&owner));
  EXPECT_EQ("something", owner);
}

// Enterprise managed, denoted by management_mode.
TEST(DevicePolicyImplTest, IsEnterpriseManaged_ManagementModeManaged) {
  em::PolicyData policy_data;
  policy_data.set_management_mode(em::PolicyData::ENTERPRISE_MANAGED);
  DevicePolicyImpl device_policy;
  device_policy.set_policy_data_for_testing(policy_data);

  EXPECT_TRUE(device_policy.IsEnterpriseManaged());
}

// Enterprise managed, fallback to DM token.
TEST(DevicePolicyImplTest, IsEnterpriseManaged_DMTokenManaged) {
  em::PolicyData policy_data;
  policy_data.set_request_token("abc");
  DevicePolicyImpl device_policy;
  device_policy.set_policy_data_for_testing(policy_data);

  EXPECT_TRUE(device_policy.IsEnterpriseManaged());
}

// Consumer owned, denoted by management_mode.
TEST(DevicePolicyImplTest, IsEnterpriseManaged_ManagementModeConsumer) {
  em::PolicyData policy_data;
  policy_data.set_management_mode(em::PolicyData::LOCAL_OWNER);
  policy_data.set_request_token("codepath-must-ignore-dmtoken");
  DevicePolicyImpl device_policy;
  device_policy.set_policy_data_for_testing(policy_data);

  EXPECT_FALSE(device_policy.IsEnterpriseManaged());
}

// Consumer owned, fallback to interpreting absence of DM token.
TEST(DevicePolicyImplTest, IsEnterpriseManaged_DMTokenConsumer) {
  em::PolicyData policy_data;
  DevicePolicyImpl device_policy;
  device_policy.set_policy_data_for_testing(policy_data);

  EXPECT_FALSE(device_policy.IsEnterpriseManaged());
}

}  // namespace policy
