// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device_local_account_policy_service.h"

#include <algorithm>

#include <base/basictypes.h>
#include <base/compiler_specific.h>
#include <base/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/memory/scoped_ptr.h>
#include <base/message_loop.h>
#include <base/message_loop_proxy.h>
#include <base/run_loop.h>
#include <chromeos/cryptohome.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "login_manager/chrome_device_policy.pb.h"
#include "login_manager/mock_policy_key.h"
#include "login_manager/mock_policy_service.h"
#include "login_manager/mock_policy_store.h"

namespace em = enterprise_management;

using testing::Return;
using testing::StrictMock;
using testing::_;

namespace login_manager {

class DeviceLocalAccountPolicyServiceTest : public ::testing::Test {
 public:
  DeviceLocalAccountPolicyServiceTest()
      : fake_account_("account@example.com") {
  }

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    FilePath salt_path = temp_dir_.path().Append("salt");
    ASSERT_EQ(0, file_util::WriteFile(salt_path, NULL, 0));
    chromeos::cryptohome::home::SetSystemSaltPath(salt_path.value());

    fake_account_policy_path_ =
        temp_dir_.path()
            .Append(chromeos::cryptohome::home::SanitizeUserName(fake_account_))
            .Append(DeviceLocalAccountPolicyService::kPolicyDir)
            .Append(DeviceLocalAccountPolicyService::kPolicyFileName);

    em::PolicyFetchResponse policy_proto;
    policy_proto.set_policy_data("policy-data");
    policy_proto.set_policy_data_signature("policy-data-signature");
    ASSERT_TRUE(policy_proto.SerializeToString(&policy_blob_));

    scoped_refptr<base::MessageLoopProxy> message_loop(
        base::MessageLoopProxy::current());
    service_.reset(new DeviceLocalAccountPolicyService(temp_dir_.path(),
                                                 &key_,
                                                 message_loop));
  }

  void SetupAccount() {
    em::ChromeDeviceSettingsProto device_settings;
    em::DeviceLocalAccountInfoProto* account =
        device_settings.mutable_device_local_accounts()->add_account();
    account->set_type(
        em::DeviceLocalAccountInfoProto::ACCOUNT_TYPE_PUBLIC_SESSION);
    account->set_account_id(fake_account_);
    service_->UpdateDeviceSettings(device_settings);
  }

  void SetupKey() {
    EXPECT_CALL(key_, PopulateFromDiskIfPossible()).Times(0);
    EXPECT_CALL(key_, IsPopulated())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(key_, Verify(_, _, _, _))
        .WillRepeatedly(Return(true));
  }

 protected:
  const std::string fake_account_;
  FilePath fake_account_policy_path_;

  std::string policy_blob_;

  MessageLoop loop_;
  base::ScopedTempDir temp_dir_;

  MockPolicyKey key_;
  MockPolicyServiceCompletion completion_;

  scoped_ptr<DeviceLocalAccountPolicyService> service_;

  DISALLOW_COPY_AND_ASSIGN(DeviceLocalAccountPolicyServiceTest);
};

TEST_F(DeviceLocalAccountPolicyServiceTest, StoreInvalidAccount) {
  EXPECT_CALL(completion_, Failure(_));
  EXPECT_FALSE(
      service_->Store(fake_account_,
                      reinterpret_cast<const uint8*>(policy_blob_.c_str()),
                      policy_blob_.size(), &completion_));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(file_util::PathExists(fake_account_policy_path_));
}

TEST_F(DeviceLocalAccountPolicyServiceTest, StoreSuccess) {
  SetupAccount();
  SetupKey();

  EXPECT_CALL(completion_, Success());
  EXPECT_TRUE(
      service_->Store(fake_account_,
                      reinterpret_cast<const uint8*>(policy_blob_.c_str()),
                      policy_blob_.size(), &completion_));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(file_util::PathExists(fake_account_policy_path_));
}

TEST_F(DeviceLocalAccountPolicyServiceTest, StoreBadPolicy) {
  SetupAccount();
  SetupKey();

  policy_blob_ = "bad!";

  EXPECT_CALL(completion_, Failure(_));
  EXPECT_FALSE(
      service_->Store(fake_account_,
                      reinterpret_cast<const uint8*>(policy_blob_.c_str()),
                      policy_blob_.size(), &completion_));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(file_util::PathExists(fake_account_policy_path_));
}

TEST_F(DeviceLocalAccountPolicyServiceTest, StoreBadSignature) {
  SetupAccount();
  SetupKey();
  EXPECT_CALL(key_, Verify(_, _, _, _))
      .WillRepeatedly(Return(false));

  EXPECT_CALL(completion_, Failure(_));
  EXPECT_FALSE(
      service_->Store(fake_account_,
                      reinterpret_cast<const uint8*>(policy_blob_.c_str()),
                      policy_blob_.size(), &completion_));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(file_util::PathExists(fake_account_policy_path_));
}

TEST_F(DeviceLocalAccountPolicyServiceTest, StoreNoRotation) {
  em::PolicyFetchResponse policy_proto;
  policy_proto.set_policy_data("policy-data");
  policy_proto.set_policy_data_signature("policy-data-signature");
  policy_proto.set_new_public_key("new-public-key");
  policy_proto.set_new_public_key_signature("new-public-key-signature");
  ASSERT_TRUE(policy_proto.SerializeToString(&policy_blob_));

  SetupAccount();
  SetupKey();

  // No key modifications.
  EXPECT_CALL(key_, Equals(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(key_, PopulateFromBuffer(_)).Times(0);
  EXPECT_CALL(key_, PopulateFromKeypair(_)).Times(0);
  EXPECT_CALL(key_, Rotate(_, _)).Times(0);
  EXPECT_CALL(key_, ClobberCompromisedKey(_)).Times(0);

  EXPECT_CALL(completion_, Failure(_));
  EXPECT_FALSE(
      service_->Store(fake_account_,
                      reinterpret_cast<const uint8*>(policy_blob_.c_str()),
                      policy_blob_.size(), &completion_));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(file_util::PathExists(fake_account_policy_path_));
}

TEST_F(DeviceLocalAccountPolicyServiceTest, RetrieveInvalidAccount) {
  SetupKey();

  std::vector<uint8> policy_data;
  EXPECT_FALSE(service_->Retrieve(fake_account_, &policy_data));
  EXPECT_TRUE(policy_data.empty());
}

TEST_F(DeviceLocalAccountPolicyServiceTest, RetrieveNoPolicy) {
  SetupAccount();
  SetupKey();

  std::vector<uint8> policy_data;
  EXPECT_TRUE(service_->Retrieve(fake_account_, &policy_data));
  EXPECT_TRUE(policy_data.empty());
}

TEST_F(DeviceLocalAccountPolicyServiceTest, RetrieveSuccess) {
  SetupAccount();
  SetupKey();

  ASSERT_TRUE(file_util::CreateDirectory(fake_account_policy_path_.DirName()));
  ASSERT_EQ(policy_blob_.size(),
            file_util::WriteFile(fake_account_policy_path_,
                                 policy_blob_.c_str(), policy_blob_.size()));

  std::vector<uint8> policy_data;
  EXPECT_TRUE(service_->Retrieve(fake_account_, &policy_data));
  EXPECT_FALSE(policy_data.empty());
}

TEST_F(DeviceLocalAccountPolicyServiceTest, PurgeStaleAccounts) {
  SetupKey();

  ASSERT_TRUE(file_util::WriteFile(fake_account_policy_path_,
                                   policy_blob_.c_str(), policy_blob_.size()));

  em::ChromeDeviceSettingsProto device_settings;
  service_->UpdateDeviceSettings(device_settings);
  EXPECT_FALSE(file_util::PathExists(fake_account_policy_path_));
}

TEST_F(DeviceLocalAccountPolicyServiceTest, MigrateUppercaseDirs) {
  const char *kDir1 = "356a192b7913b04c54574d18c28d46e6395428ab";
  const char *kDir2 = "DA4B9237BACCCDF19C0760CAB7AEC4A8359010B0";
  const char *kDir2Lower = "da4b9237bacccdf19c0760cab7aec4a8359010b0";
  const char *kUnrelated = "foobar";

  FilePath fp1(temp_dir_.path().Append(kDir1));
  FilePath fp2(temp_dir_.path().Append(kDir2));
  FilePath fp2lower(temp_dir_.path().Append(kDir2Lower));
  FilePath fpunrel(temp_dir_.path().Append(kUnrelated));

  EXPECT_TRUE(file_util::CreateDirectory(fp1));
  EXPECT_TRUE(file_util::CreateDirectory(fp2));
  EXPECT_TRUE(file_util::CreateDirectory(fpunrel));

  EXPECT_TRUE(service_->MigrateUppercaseDirs());

  EXPECT_TRUE(file_util::DirectoryExists(fp1));
  EXPECT_FALSE(file_util::DirectoryExists(fp2));
  EXPECT_TRUE(file_util::DirectoryExists(fp2lower));
  EXPECT_TRUE(file_util::DirectoryExists(fpunrel));
}

TEST_F(DeviceLocalAccountPolicyServiceTest, LegacyPublicSessionIdFallback) {
  // Check that a legacy public session ID continues to work as long as the
  // account_id / type fields are not present.
  em::ChromeDeviceSettingsProto device_settings;
  em::DeviceLocalAccountInfoProto* account =
      device_settings.mutable_device_local_accounts()->add_account();
  account->set_deprecated_public_session_id(fake_account_);
  service_->UpdateDeviceSettings(device_settings);
  SetupKey();

  EXPECT_CALL(completion_, Success());
  EXPECT_TRUE(
      service_->Store(fake_account_,
                      reinterpret_cast<const uint8*>(policy_blob_.c_str()),
                      policy_blob_.size(), &completion_));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(file_util::PathExists(fake_account_policy_path_));

  std::vector<uint8> policy_data;
  EXPECT_TRUE(service_->Retrieve(fake_account_, &policy_data));

  ASSERT_EQ(policy_blob_.size(), policy_data.size());
  EXPECT_TRUE(std::equal(policy_blob_.begin(), policy_blob_.end(),
                         policy_data.begin()));
}

TEST_F(DeviceLocalAccountPolicyServiceTest, LegacyPublicSessionIdIgnored) {
  // If there's a legacy public session ID and an account id / type pair, the
  // former should get ignored.
  const char kDeprecatedId[] = "deprecated";
  em::ChromeDeviceSettingsProto device_settings;
  em::DeviceLocalAccountInfoProto* account =
      device_settings.mutable_device_local_accounts()->add_account();
  account->set_deprecated_public_session_id(kDeprecatedId);
  account->set_type(
      em::DeviceLocalAccountInfoProto::ACCOUNT_TYPE_PUBLIC_SESSION);
  account->set_account_id(fake_account_);
  service_->UpdateDeviceSettings(device_settings);
  SetupKey();

  EXPECT_CALL(completion_, Failure(_));
  EXPECT_FALSE(
      service_->Store(kDeprecatedId,
                      reinterpret_cast<const uint8*>(policy_blob_.c_str()),
                      policy_blob_.size(), &completion_));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(file_util::PathExists(fake_account_policy_path_));
}

}  // namespace login_manager
