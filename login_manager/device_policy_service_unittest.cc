// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/device_policy_service.h"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include <base/basictypes.h>
#include <base/file_path.h>
#include <base/file_util.h>
#include <base/memory/scoped_temp_dir.h>
#include <base/message_loop.h>
#include <base/message_loop_proxy.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "login_manager/bindings/chrome_device_policy.pb.h"
#include "login_manager/bindings/device_management_backend.pb.h"
#include "login_manager/mock_mitigator.h"
#include "login_manager/mock_nss_util.h"
#include "login_manager/mock_owner_key.h"
#include "login_manager/mock_policy_service.h"
#include "login_manager/mock_policy_store.h"

namespace em = enterprise_management;

using google::protobuf::RepeatedPtrField;

using testing::AnyNumber;
using testing::AtLeast;
using testing::DoAll;
using testing::Expectation;
using testing::Mock;
using testing::Return;
using testing::ReturnRef;
using testing::Sequence;
using testing::StrictMock;
using testing::WithArg;
using testing::_;

ACTION_P(AssignVector, str) {
  arg0->assign(str.begin(), str.end());
}

namespace login_manager {

class DevicePolicyServiceTest : public ::testing::Test {
 public:
  DevicePolicyServiceTest()
      : owner_("user@somewhere"),
        fake_sig_("fake_signature"),
        fake_key_("fake_key"),
        new_fake_sig_("new_fake_signature"),
        key_(NULL),
        store_(NULL) {
    fake_key_vector_.assign(fake_key_.begin(), fake_key_.end());
  }

  virtual void SetUp() {
    ASSERT_TRUE(tmpdir_.CreateUniqueTempDir());
    serial_recovery_flag_file_ =
        tmpdir_.path().AppendASCII("serial_recovery_flag");
  }

  void InitPolicy(const em::ChromeDeviceSettingsProto& settings,
                  const std::string& owner,
                  const std::string& signature,
                  const std::string& request_token,
                  bool valid_serial_number_missing) {
    std::string settings_str;
    ASSERT_TRUE(settings.SerializeToString(&settings_str));

    em::PolicyData policy_data;
    policy_data.set_policy_type(DevicePolicyService::kDevicePolicyType);
    policy_data.set_policy_value(settings_str);
    if (!owner.empty())
      policy_data.set_username(owner);
    if (!request_token.empty())
      policy_data.set_request_token(request_token);
    if (valid_serial_number_missing)
      policy_data.set_valid_serial_number_missing(true);
    std::string policy_data_str;
    ASSERT_TRUE(policy_data.SerializeToString(&policy_data_str));

    policy_proto_.Clear();
    policy_proto_.set_policy_data(policy_data_str);
    policy_proto_.set_policy_data_signature(signature);
    ASSERT_TRUE(policy_proto_.SerializeToString(&policy_str_));
  }

  void InitEmptyPolicy(const std::string& owner,
                       const std::string& signature,
                       const std::string& request_token) {
    em::ChromeDeviceSettingsProto settings;
    InitPolicy(settings, owner, signature, request_token, false);
  }

  void InitService(NssUtil* nss) {
    key_ = new StrictMock<MockOwnerKey>;
    store_ = new StrictMock<MockPolicyStore>;
    mitigator_.reset(new MockMitigator);
    scoped_refptr<base::MessageLoopProxy> message_loop(
        base::MessageLoopProxy::CreateForCurrentThread());
    service_ = new DevicePolicyService(serial_recovery_flag_file_,
                                       store_, key_,
                                       message_loop,
                                       nss,
                                       mitigator_.get());

    // Allow the key to be read any time.
    EXPECT_CALL(*key_, public_key_der())
        .WillRepeatedly(ReturnRef(fake_key_vector_));
  }

  void RecordNewPolicy(const em::PolicyFetchResponse& policy) {
    new_policy_proto_.CopyFrom(policy);
  }

  // Checks that the recorded |new_policy_| has been modified accordingly from
  // |policy_proto_| while logging in |owner_| as a device owner.
  void CheckNewOwnerSettings(
      const em::ChromeDeviceSettingsProto& old_settings) {
    // Check PolicyFetchResponse wrapper.
    ASSERT_EQ(new_fake_sig_, new_policy_proto_.policy_data_signature());
    ASSERT_EQ(fake_key_, new_policy_proto_.new_public_key());
    ASSERT_EQ(std::string(), new_policy_proto_.new_public_key_signature());
    ASSERT_TRUE(new_policy_proto_.has_policy_data());

    // Check signed policy data.
    em::PolicyData policy_data;
    ASSERT_TRUE(policy_data.ParseFromString(new_policy_proto_.policy_data()));
    ASSERT_EQ(DevicePolicyService::kDevicePolicyType,
              policy_data.policy_type());
    ASSERT_FALSE(policy_data.has_request_token());
    ASSERT_TRUE(policy_data.has_policy_value());
    ASSERT_EQ(policy_data.username(), owner_);

    // Check the device settings.
    em::ChromeDeviceSettingsProto settings;
    ASSERT_TRUE(settings.ParseFromString(policy_data.policy_value()));
    ASSERT_TRUE(settings.has_user_whitelist());
    const RepeatedPtrField<std::string>& old_whitelist(
        old_settings.user_whitelist().user_whitelist());
    const RepeatedPtrField<std::string>& whitelist(
        settings.user_whitelist().user_whitelist());
    ASSERT_LE(old_whitelist.size(), whitelist.size());
    ASSERT_GE(old_whitelist.size() + 1, whitelist.size());
    std::set<std::string> old_whitelist_set(old_whitelist.begin(),
                                            old_whitelist.end());
    old_whitelist_set.insert(owner_);
    std::set<std::string> whitelist_set(whitelist.begin(),
                                        whitelist.end());
    ASSERT_EQ(old_whitelist_set.size(), whitelist_set.size());
    ASSERT_TRUE(std::equal(whitelist_set.begin(), whitelist_set.end(),
                           old_whitelist_set.begin()));
    ASSERT_TRUE(settings.has_allow_new_users());

    // Make sure no other fields have been touched.
    settings.clear_user_whitelist();
    settings.clear_allow_new_users();
    em::ChromeDeviceSettingsProto blank_settings;
    ASSERT_EQ(settings.SerializeAsString(), blank_settings.SerializeAsString());
  }

  void ExpectMitigating(bool mitigating) {
    EXPECT_CALL(*mitigator_, Mitigating())
        .WillRepeatedly(Return(mitigating));
  }

  void ExpectGetPolicy(Sequence sequence,
                       const em::PolicyFetchResponse& policy) {
    EXPECT_CALL(*store_, Get())
        .InSequence(sequence)
        .WillRepeatedly(ReturnRef(policy));
  }

  void ExpectInstallNewOwnerPolicy(Sequence sequence) {
    Expectation get_policy =
        EXPECT_CALL(*store_, Get())
            .WillRepeatedly(ReturnRef(policy_proto_));
    Expectation compare_keys =
        EXPECT_CALL(*key_, Equals(_))
            .WillRepeatedly(Return(false));
    Expectation sign =
        EXPECT_CALL(*key_, Sign(_, _, _))
            .After(get_policy)
            .WillOnce(DoAll(WithArg<2>(AssignVector(new_fake_sig_)),
                            Return(true)));
    Expectation set_policy = EXPECT_CALL(*store_, Set(_))
        .InSequence(sequence)
        .After(sign, compare_keys)
        .WillOnce(Invoke(this, &DevicePolicyServiceTest::RecordNewPolicy));
  }

  void ExpectFailedInstallNewOwnerPolicy(Sequence sequence) {
    Expectation get_policy =
        EXPECT_CALL(*store_, Get())
            .WillRepeatedly(ReturnRef(policy_proto_));
    Expectation compare_keys =
        EXPECT_CALL(*key_, Equals(_))
            .WillRepeatedly(Return(false));
    Expectation sign =
        EXPECT_CALL(*key_, Sign(_, _, _))
            .After(get_policy)
            .WillOnce(DoAll(WithArg<2>(AssignVector(new_fake_sig_)),
                            Return(false)));
  }

  void ExpectPersistKeyAndPolicy() {
    Mock::VerifyAndClearExpectations(key_);
    Mock::VerifyAndClearExpectations(store_);

    EXPECT_CALL(*key_, Persist())
        .WillOnce(Return(true));
    EXPECT_CALL(*store_, Persist())
        .WillOnce(Return(true));
    loop_.RunAllPending();
  }

  void ExpectNoPersistKeyAndPolicy() {
    Mock::VerifyAndClearExpectations(key_);
    Mock::VerifyAndClearExpectations(store_);

    EXPECT_CALL(*key_, Persist()).Times(0);
    EXPECT_CALL(*store_, Persist()).Times(0);
    loop_.RunAllPending();
  }

  em::PolicyFetchResponse policy_proto_;
  std::string policy_str_;

  em::PolicyFetchResponse new_policy_proto_;

  std::string owner_;
  std::string fake_sig_;
  std::string fake_key_;
  std::vector<uint8> fake_key_vector_;
  std::string new_fake_sig_;

  MessageLoop loop_;

  ScopedTempDir tmpdir_;
  FilePath serial_recovery_flag_file_;

  // Use StrictMock to make sure that no unexpected policy or key mutations can
  // occur without the test failing.
  StrictMock<MockOwnerKey>* key_;
  StrictMock<MockPolicyStore>* store_;
  scoped_ptr<MockMitigator> mitigator_;
  MockPolicyServiceCompletion completion_;

  scoped_refptr<DevicePolicyService> service_;
};

TEST_F(DevicePolicyServiceTest, CheckAndHandleOwnerLogin_SuccessEmptyPolicy) {
  InitService(new KeyCheckUtil);
  em::ChromeDeviceSettingsProto settings;
  ASSERT_NO_FATAL_FAILURE(InitPolicy(settings, owner_, fake_sig_, "", false));

  Sequence s;
  ExpectGetPolicy(s, policy_proto_);
  ExpectInstallNewOwnerPolicy(s);
  ExpectGetPolicy(s, new_policy_proto_);
  EXPECT_CALL(*mitigator_, Mitigate(_))
      .Times(0);

  PolicyService::Error error;
  bool is_owner = false;
  EXPECT_TRUE(service_->CheckAndHandleOwnerLogin(owner_,
                                                 &is_owner,
                                                 &error));
  EXPECT_TRUE(is_owner);
  EXPECT_NO_FATAL_FAILURE(CheckNewOwnerSettings(settings));
}

TEST_F(DevicePolicyServiceTest, CheckAndHandleOwnerLogin_SuccessAddOwner) {
  InitService(new KeyCheckUtil);
  em::ChromeDeviceSettingsProto settings;
  settings.mutable_user_whitelist()->add_user_whitelist("a@b");
  settings.mutable_user_whitelist()->add_user_whitelist("c@d");
  ASSERT_NO_FATAL_FAILURE(InitPolicy(settings, owner_, fake_sig_, "", false));

  Sequence s;
  ExpectGetPolicy(s, policy_proto_);
  ExpectInstallNewOwnerPolicy(s);
  ExpectGetPolicy(s, new_policy_proto_);
  EXPECT_CALL(*mitigator_, Mitigate(_))
      .Times(0);

  PolicyService::Error error;
  bool is_owner = false;
  EXPECT_TRUE(service_->CheckAndHandleOwnerLogin(owner_,
                                                 &is_owner,
                                                 &error));
  EXPECT_TRUE(is_owner);
  EXPECT_NO_FATAL_FAILURE(CheckNewOwnerSettings(settings));
}

TEST_F(DevicePolicyServiceTest, CheckAndHandleOwnerLogin_SuccessOwnerPresent) {
  InitService(new KeyCheckUtil);
  em::ChromeDeviceSettingsProto settings;
  settings.mutable_user_whitelist()->add_user_whitelist("a@b");
  settings.mutable_user_whitelist()->add_user_whitelist("c@d");
  settings.mutable_user_whitelist()->add_user_whitelist(owner_);
  settings.mutable_allow_new_users()->set_allow_new_users(true);
  ASSERT_NO_FATAL_FAILURE(InitPolicy(settings, owner_, fake_sig_, "", false));

  Sequence s;
  ExpectGetPolicy(s, policy_proto_);
  ExpectInstallNewOwnerPolicy(s);
  ExpectGetPolicy(s, new_policy_proto_);
  EXPECT_CALL(*mitigator_, Mitigate(_))
      .Times(0);

  PolicyService::Error error;
  bool is_owner = false;
  EXPECT_TRUE(service_->CheckAndHandleOwnerLogin(owner_,
                                                 &is_owner,
                                                 &error));
  EXPECT_TRUE(is_owner);
  EXPECT_NO_FATAL_FAILURE(CheckNewOwnerSettings(settings));
}

TEST_F(DevicePolicyServiceTest, CheckAndHandleOwnerLogin_NotOwner) {
  InitService(new KeyFailUtil);
  ASSERT_NO_FATAL_FAILURE(InitEmptyPolicy(owner_, fake_sig_, ""));

  Sequence s;
  ExpectGetPolicy(s, policy_proto_);
  EXPECT_CALL(*mitigator_, Mitigate(_))
      .Times(0);

  PolicyService::Error error;
  bool is_owner = true;
  EXPECT_TRUE(service_->CheckAndHandleOwnerLogin("regular_user@somewhere",
                                                 &is_owner,
                                                 &error));
  EXPECT_FALSE(is_owner);
}

TEST_F(DevicePolicyServiceTest, CheckAndHandleOwnerLogin_EnterpriseDevice) {
  InitService(new KeyFailUtil);
  ASSERT_NO_FATAL_FAILURE(InitEmptyPolicy(owner_, fake_sig_, "fake_token"));

  Sequence s;
  ExpectGetPolicy(s, policy_proto_);
  EXPECT_CALL(*mitigator_, Mitigate(_))
      .Times(0);

  PolicyService::Error error;
  bool is_owner = true;
  EXPECT_TRUE(service_->CheckAndHandleOwnerLogin(owner_,
                                                 &is_owner,
                                                 &error));
  EXPECT_FALSE(is_owner);
}


TEST_F(DevicePolicyServiceTest, CheckAndHandleOwnerLogin_MissingKey) {
  InitService(new KeyFailUtil);
  ASSERT_NO_FATAL_FAILURE(InitEmptyPolicy(owner_, fake_sig_, ""));

  Sequence s;
  ExpectGetPolicy(s, policy_proto_);
  EXPECT_CALL(*mitigator_, Mitigate(_))
      .InSequence(s)
      .WillOnce(Return(true));

  PolicyService::Error error;
  bool is_owner = false;
  EXPECT_TRUE(service_->CheckAndHandleOwnerLogin(owner_,
                                                 &is_owner,
                                                 &error));
  EXPECT_TRUE(is_owner);
}

TEST_F(DevicePolicyServiceTest, CheckAndHandleOwnerLogin_MitigationFailure) {
  InitService(new KeyFailUtil);
  ASSERT_NO_FATAL_FAILURE(InitEmptyPolicy(owner_, fake_sig_, ""));

  Sequence s;
  ExpectGetPolicy(s, policy_proto_);
  EXPECT_CALL(*mitigator_, Mitigate(_))
      .InSequence(s)
      .WillOnce(Return(false));

  PolicyService::Error error;
  bool is_owner = false;
  EXPECT_FALSE(service_->CheckAndHandleOwnerLogin(owner_,
                                                  &is_owner,
                                                  &error));
}

TEST_F(DevicePolicyServiceTest, CheckAndHandleOwnerLogin_SigningFailure) {
  InitService(new KeyCheckUtil);
  em::ChromeDeviceSettingsProto settings;
  ASSERT_NO_FATAL_FAILURE(InitPolicy(settings, owner_, fake_sig_, "", false));

  Sequence s;
  ExpectGetPolicy(s, policy_proto_);

  EXPECT_CALL(*store_, Get())
      .WillRepeatedly(ReturnRef(policy_proto_));
  Expectation compare_keys =
      EXPECT_CALL(*key_, Equals(_))
          .WillRepeatedly(Return(false));
  Expectation sign =
      EXPECT_CALL(*key_, Sign(_, _, _))
          .After(compare_keys)
          .WillOnce(Return(false));

  ExpectGetPolicy(s, new_policy_proto_);
  EXPECT_CALL(*mitigator_, Mitigate(_))
      .Times(0);

  PolicyService::Error error;
  bool is_owner = false;
  EXPECT_FALSE(service_->CheckAndHandleOwnerLogin(owner_,
                                                  &is_owner,
                                                  &error));
}

TEST_F(DevicePolicyServiceTest, ValidateAndStoreOwnerKey_SuccessNewKey) {
  InitService(new KeyCheckUtil);

  ExpectMitigating(false);

  Sequence s;
  ExpectGetPolicy(s, policy_proto_);
  EXPECT_CALL(*key_, PopulateFromBuffer(fake_key_vector_))
      .InSequence(s)
      .WillOnce(Return(true));
  ExpectInstallNewOwnerPolicy(s);

  service_->ValidateAndStoreOwnerKey(owner_, fake_key_);

  ExpectPersistKeyAndPolicy();
}

TEST_F(DevicePolicyServiceTest, ValidateAndStoreOwnerKey_SuccessMitigating) {
  InitService(new KeyCheckUtil);

  ExpectMitigating(true);

  Sequence s;
  ExpectGetPolicy(s, policy_proto_);
  EXPECT_CALL(*key_, ClobberCompromisedKey(fake_key_vector_))
      .InSequence(s)
      .WillOnce(Return(true));
  ExpectInstallNewOwnerPolicy(s);

  service_->ValidateAndStoreOwnerKey(owner_, fake_key_);

  ExpectPersistKeyAndPolicy();
}

TEST_F(DevicePolicyServiceTest, ValidateAndStoreOwnerKey_FailedMitigating) {
  InitService(new KeyCheckUtil);

  ExpectMitigating(true);

  Sequence s;
  ExpectGetPolicy(s, policy_proto_);
  EXPECT_CALL(*key_, ClobberCompromisedKey(fake_key_vector_))
      .InSequence(s)
      .WillOnce(Return(true));
  ExpectFailedInstallNewOwnerPolicy(s);

  service_->ValidateAndStoreOwnerKey(owner_, fake_key_);

  ExpectNoPersistKeyAndPolicy();
}

TEST_F(DevicePolicyServiceTest, ValidateAndStoreOwnerKey_SuccessAddOwner) {
  InitService(new KeyCheckUtil);
  em::ChromeDeviceSettingsProto settings;
  settings.mutable_user_whitelist()->add_user_whitelist("a@b");
  settings.mutable_user_whitelist()->add_user_whitelist("c@d");
  ASSERT_NO_FATAL_FAILURE(InitPolicy(settings, owner_, fake_sig_, "", false));

  ExpectMitigating(false);

  Sequence s;
  ExpectGetPolicy(s, policy_proto_);
  EXPECT_CALL(*key_, PopulateFromBuffer(fake_key_vector_))
      .InSequence(s)
      .WillOnce(Return(true));
  ExpectInstallNewOwnerPolicy(s);

  service_->ValidateAndStoreOwnerKey(owner_, fake_key_);

  ExpectPersistKeyAndPolicy();
}

TEST_F(DevicePolicyServiceTest, ValidateAndStoreOwnerKey_NoPrivateKey) {
  InitService(new KeyFailUtil);

  service_->ValidateAndStoreOwnerKey(owner_, fake_key_);
}

TEST_F(DevicePolicyServiceTest, ValidateAndStoreOwnerKey_NewKeyInstallFails) {
  InitService(new KeyCheckUtil);

  ExpectMitigating(false);

  Sequence s;
  ExpectGetPolicy(s, policy_proto_);
  EXPECT_CALL(*key_, PopulateFromBuffer(fake_key_vector_))
      .InSequence(s)
      .WillOnce(Return(false));

  service_->ValidateAndStoreOwnerKey(owner_, fake_key_);
}

TEST_F(DevicePolicyServiceTest, ValidateAndStoreOwnerKey_KeyClobberFails) {
  InitService(new KeyCheckUtil);

  ExpectMitigating(true);

  Sequence s;
  ExpectGetPolicy(s, policy_proto_);
  EXPECT_CALL(*key_, ClobberCompromisedKey(fake_key_vector_))
      .InSequence(s)
      .WillOnce(Return(false));

  service_->ValidateAndStoreOwnerKey(owner_, fake_key_);
}

TEST_F(DevicePolicyServiceTest, ValidateAndStoreOwnerKey_NssFailure) {
  InitService(new SadNssUtil);

  service_->ValidateAndStoreOwnerKey(owner_, fake_key_);
}


TEST_F(DevicePolicyServiceTest, KeyMissing_Present) {
  InitService(new MockNssUtil);

  EXPECT_CALL(*key_, HaveCheckedDisk())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*key_, IsPopulated())
      .WillRepeatedly(Return(true));

  EXPECT_FALSE(service_->KeyMissing());
}

TEST_F(DevicePolicyServiceTest, KeyMissing_NoDiskCheck) {
  InitService(new MockNssUtil);

  EXPECT_CALL(*key_, HaveCheckedDisk())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*key_, IsPopulated())
      .WillRepeatedly(Return(false));

  EXPECT_FALSE(service_->KeyMissing());
}

TEST_F(DevicePolicyServiceTest, KeyMissing_CheckedAndMissing) {
  InitService(new MockNssUtil);

  EXPECT_CALL(*key_, HaveCheckedDisk())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*key_, IsPopulated())
      .WillRepeatedly(Return(false));

  EXPECT_TRUE(service_->KeyMissing());
}

TEST_F(DevicePolicyServiceTest, SerialRecoveryFlagFileInitialization) {
  InitService(new MockNssUtil);

  EXPECT_CALL(*key_, PopulateFromDiskIfPossible())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*key_, IsPopulated())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*store_, LoadOrCreate())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*store_, Get())
      .WillRepeatedly(ReturnRef(policy_proto_));

  em::ChromeDeviceSettingsProto settings;
  ASSERT_NO_FATAL_FAILURE(InitPolicy(settings, owner_, fake_sig_, "t", true));
  EXPECT_TRUE(service_->Initialize());
  EXPECT_TRUE(file_util::PathExists(serial_recovery_flag_file_));

  ASSERT_NO_FATAL_FAILURE(InitPolicy(settings, owner_, fake_sig_, "", false));
  EXPECT_TRUE(service_->Initialize());
  EXPECT_FALSE(file_util::PathExists(serial_recovery_flag_file_));
}

TEST_F(DevicePolicyServiceTest, SerialRecoveryFlagFileUpdating) {
  InitService(new MockNssUtil);
  em::ChromeDeviceSettingsProto settings;
  EXPECT_FALSE(file_util::PathExists(serial_recovery_flag_file_));

  EXPECT_CALL(*key_, Verify(_, _, _, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*store_, Set(_)).Times(AnyNumber());
  EXPECT_CALL(*store_, Get())
      .WillRepeatedly(ReturnRef(policy_proto_));
  EXPECT_CALL(completion_, Success()).Times(AnyNumber());
  EXPECT_CALL(completion_, Failure(_)).Times(AnyNumber());

  // Installing a policy blob that doesn't have a request token (indicates local
  // owner) should not create the file.
  ASSERT_NO_FATAL_FAILURE(InitPolicy(settings, owner_, fake_sig_, "", true));
  EXPECT_TRUE(
      service_->Store(reinterpret_cast<const uint8*>(policy_str_.c_str()),
                      policy_str_.size(), &completion_,
                      PolicyService::KEY_CLOBBER));
  EXPECT_FALSE(file_util::PathExists(serial_recovery_flag_file_));

  // Storing an enterprise policy blob with the |valid_serial_number_missing|
  // flag set should create the flag file.
  ASSERT_NO_FATAL_FAILURE(InitPolicy(settings, owner_, fake_sig_, "t", true));
  EXPECT_TRUE(
      service_->Store(reinterpret_cast<const uint8*>(policy_str_.c_str()),
                      policy_str_.size(), &completion_,
                      PolicyService::KEY_CLOBBER));
  EXPECT_TRUE(file_util::PathExists(serial_recovery_flag_file_));

  // Storing bad policy shouldn't remove the file.
  EXPECT_FALSE(service_->Store(NULL, 0, &completion_,
                               PolicyService::KEY_CLOBBER));
  EXPECT_TRUE(file_util::PathExists(serial_recovery_flag_file_));

  // Clearing the flag should remove the file.
  ASSERT_NO_FATAL_FAILURE(InitPolicy(settings, owner_, fake_sig_, "t", false));
  EXPECT_TRUE(
      service_->Store(reinterpret_cast<const uint8*>(policy_str_.c_str()),
                      policy_str_.size(), &completion_,
                      PolicyService::KEY_CLOBBER));
  EXPECT_FALSE(file_util::PathExists(serial_recovery_flag_file_));
}

}  // namespace login_manager
