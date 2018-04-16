// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "policy/device_policy_impl.h"

#include "bindings/chrome_device_policy.pb.h"
#include "install_attributes/mock_install_attributes_reader.h"

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

// RollbackAllowedMilestones is not set.
TEST(DevicePolicyImplTest, GetRollbackAllowedMilestones_NotSet) {
  DevicePolicyImpl device_policy;
  device_policy.set_install_attributes_for_testing(
      std::make_unique<MockInstallAttributesReader>(
          InstallAttributesReader::kDeviceModeEnterprise, true));

  int value = -1;
  ASSERT_TRUE(device_policy.GetRollbackAllowedMilestones(&value));
  EXPECT_EQ(4, value);
}

// RollbackAllowedMilestones is set to a valid value.
TEST(DevicePolicyImplTest, GetRollbackAllowedMilestones_Set) {
  em::ChromeDeviceSettingsProto device_policy_proto;
  em::AutoUpdateSettingsProto* auto_update_settings =
      device_policy_proto.mutable_auto_update_settings();
  auto_update_settings->set_rollback_allowed_milestones(3);
  DevicePolicyImpl device_policy;
  device_policy.set_policy_for_testing(device_policy_proto);
  device_policy.set_install_attributes_for_testing(
      std::make_unique<MockInstallAttributesReader>(
          InstallAttributesReader::kDeviceModeEnterprise, true));

  int value = -1;
  ASSERT_TRUE(device_policy.GetRollbackAllowedMilestones(&value));
  EXPECT_EQ(3, value);
}

// RollbackAllowedMilestones is set to a valid value, using AD.
TEST(DevicePolicyImplTest, GetRollbackAllowedMilestones_SetAD) {
  em::ChromeDeviceSettingsProto device_policy_proto;
  em::AutoUpdateSettingsProto* auto_update_settings =
      device_policy_proto.mutable_auto_update_settings();
  auto_update_settings->set_rollback_allowed_milestones(3);
  DevicePolicyImpl device_policy;
  device_policy.set_policy_for_testing(device_policy_proto);
  device_policy.set_install_attributes_for_testing(
      std::make_unique<MockInstallAttributesReader>(
          InstallAttributesReader::kDeviceModeEnterpriseAD, true));

  int value = -1;
  ASSERT_TRUE(device_policy.GetRollbackAllowedMilestones(&value));
  EXPECT_EQ(3, value);
}

// RollbackAllowedMilestones is set to a valid value, but it's not an enterprise
// device.
TEST(DevicePolicyImplTest, GetRollbackAllowedMilestones_SetConsumer) {
  em::ChromeDeviceSettingsProto device_policy_proto;
  em::AutoUpdateSettingsProto* auto_update_settings =
      device_policy_proto.mutable_auto_update_settings();
  auto_update_settings->set_rollback_allowed_milestones(3);
  DevicePolicyImpl device_policy;
  device_policy.set_policy_for_testing(device_policy_proto);
  device_policy.set_install_attributes_for_testing(
      std::make_unique<MockInstallAttributesReader>(
          InstallAttributesReader::kDeviceModeConsumer, true));

  int value = -1;
  ASSERT_FALSE(device_policy.GetRollbackAllowedMilestones(&value));
}

// RollbackAllowedMilestones is set to an invalid value.
TEST(DevicePolicyImplTest, GetRollbackAllowedMilestones_SetTooLarge) {
  em::ChromeDeviceSettingsProto device_policy_proto;
  em::AutoUpdateSettingsProto* auto_update_settings =
      device_policy_proto.mutable_auto_update_settings();
  auto_update_settings->set_rollback_allowed_milestones(10);
  DevicePolicyImpl device_policy;
  device_policy.set_policy_for_testing(device_policy_proto);
  device_policy.set_install_attributes_for_testing(
      std::make_unique<MockInstallAttributesReader>(
          InstallAttributesReader::kDeviceModeEnterprise, true));

  int value = -1;
  ASSERT_TRUE(device_policy.GetRollbackAllowedMilestones(&value));
  EXPECT_EQ(4, value);
}

// RollbackAllowedMilestones is set to an invalid value.
TEST(DevicePolicyImplTest, GetRollbackAllowedMilestones_SetTooSmall) {
  em::ChromeDeviceSettingsProto device_policy_proto;
  em::AutoUpdateSettingsProto* auto_update_settings =
      device_policy_proto.mutable_auto_update_settings();
  auto_update_settings->set_rollback_allowed_milestones(-1);
  DevicePolicyImpl device_policy;
  device_policy.set_policy_for_testing(device_policy_proto);
  device_policy.set_install_attributes_for_testing(
      std::make_unique<MockInstallAttributesReader>(
          InstallAttributesReader::kDeviceModeEnterprise, true));

  int value = -1;
  ASSERT_TRUE(device_policy.GetRollbackAllowedMilestones(&value));
  EXPECT_EQ(0, value);
}

}  // namespace policy
