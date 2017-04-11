// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/user_policy_service.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/run_loop.h>
#include <brillo/message_loops/fake_message_loop.h>
#include <chromeos/dbus/service_constants.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "bindings/device_management_backend.pb.h"
#include "login_manager/matchers.h"
#include "login_manager/mock_policy_key.h"
#include "login_manager/mock_policy_service.h"
#include "login_manager/mock_policy_store.h"
#include "login_manager/system_utils_impl.h"

namespace em = enterprise_management;

using ::testing::ElementsAre;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::Sequence;
using ::testing::StrictMock;
using ::testing::_;

namespace login_manager {

class UserPolicyServiceTest : public ::testing::Test {
 public:
  UserPolicyServiceTest()
      : fake_signature_("fake_signature"), policy_data_(NULL), policy_len_(0) {}

  virtual void SetUp() {
    fake_loop_.SetAsCurrent();
    ASSERT_TRUE(tmpdir_.CreateUniqueTempDir());
    key_copy_file_ = tmpdir_.path().Append("hash/key_copy.pub");

    key_ = new StrictMock<MockPolicyKey>;
    store_ = new StrictMock<MockPolicyStore>;
    service_.reset(new UserPolicyService(std::unique_ptr<PolicyStore>(store_),
                                         std::unique_ptr<PolicyKey>(key_),
                                         key_copy_file_,
                                         &system_utils_));
  }

  void InitPolicy(em::PolicyData::AssociationState state,
                  const std::string& signature) {
    em::PolicyData policy_data;
    policy_data.set_state(state);

    policy_proto_.Clear();
    ASSERT_TRUE(
        policy_data.SerializeToString(policy_proto_.mutable_policy_data()));
    if (!signature.empty())
      policy_proto_.set_policy_data_signature(signature);

    ASSERT_TRUE(policy_proto_.SerializeToString(&policy_str_));
    policy_data_ = reinterpret_cast<const uint8_t*>(policy_str_.c_str());
    policy_len_ = policy_str_.size();
  }

  void ExpectStorePolicy(const Sequence& sequence) {
    EXPECT_CALL(*store_, Set(PolicyStrEq(policy_str_))).InSequence(sequence);
    EXPECT_CALL(*store_, Persist()).InSequence(sequence).WillOnce(Return(true));
    EXPECT_CALL(*this, HandleCompletion(PolicyErrorEq(dbus_error::kNone)))
        .InSequence(sequence);
    completion_ =  base::Bind(&UserPolicyServiceTest::HandleCompletion,
                              base::Unretained(this));
  }

 protected:
  SystemUtilsImpl system_utils_;
  base::ScopedTempDir tmpdir_;
  base::FilePath key_copy_file_;

  const std::string fake_signature_;

  // Various representations of the policy protobuf.
  em::PolicyFetchResponse policy_proto_;
  std::string policy_str_;
  const uint8_t* policy_data_;
  uint32_t policy_len_;

  brillo::FakeMessageLoop fake_loop_{nullptr};

  // Use StrictMock to make sure that no unexpected policy or key mutations can
  // occur without the test failing.
  StrictMock<MockPolicyKey>* key_;
  StrictMock<MockPolicyStore>* store_;
  PolicyService::Completion completion_;

  std::unique_ptr<UserPolicyService> service_;

 private:
  MOCK_METHOD1(HandleCompletion, void(const PolicyService::Error&));

  DISALLOW_COPY_AND_ASSIGN(UserPolicyServiceTest);
};

TEST_F(UserPolicyServiceTest, StoreSignedPolicy) {
  InitPolicy(em::PolicyData::ACTIVE, fake_signature_);

  Sequence s1;
  EXPECT_CALL(*key_, Verify(_, _, _, _)).InSequence(s1).WillOnce(Return(true));
  ExpectStorePolicy(s1);

  EXPECT_TRUE(service_->Store(policy_data_, policy_len_, completion_,
                              PolicyService::KEY_NONE,
                              SignatureCheck::kEnabled));
  fake_loop_.Run();
}

TEST_F(UserPolicyServiceTest, StoreUnmanagedSigned) {
  InitPolicy(em::PolicyData::UNMANAGED, fake_signature_);

  Sequence s1;
  EXPECT_CALL(*key_, Verify(_, _, _, _)).InSequence(s1).WillOnce(Return(true));
  ExpectStorePolicy(s1);

  EXPECT_TRUE(service_->Store(policy_data_, policy_len_, completion_,
                              PolicyService::KEY_NONE,
                              SignatureCheck::kEnabled));
  fake_loop_.Run();
}

TEST_F(UserPolicyServiceTest, StoreUnmanagedKeyPresent) {
  InitPolicy(em::PolicyData::UNMANAGED, "");

  Sequence s1;
  ExpectStorePolicy(s1);
  std::vector<uint8_t> key_value;
  key_value.push_back(0x12);

  EXPECT_CALL(*key_, IsPopulated()).WillRepeatedly(Return(true));
  EXPECT_CALL(*key_, public_key_der()).WillRepeatedly(ReturnRef(key_value));

  Sequence s2;
  EXPECT_CALL(*key_, ClobberCompromisedKey(ElementsAre())).InSequence(s2);
  EXPECT_CALL(*key_, Persist()).InSequence(s2).WillOnce(Return(true));

  EXPECT_FALSE(base::PathExists(key_copy_file_));
  EXPECT_TRUE(service_->Store(policy_data_, policy_len_, completion_,
                              PolicyService::KEY_NONE,
                              SignatureCheck::kEnabled));
  fake_loop_.Run();

  EXPECT_TRUE(base::PathExists(key_copy_file_));
  std::string content;
  EXPECT_TRUE(base::ReadFileToString(key_copy_file_, &content));
  ASSERT_EQ(1u, content.size());
  EXPECT_EQ(key_value[0], content[0]);
}

TEST_F(UserPolicyServiceTest, StoreUnmanagedNoKey) {
  InitPolicy(em::PolicyData::UNMANAGED, "");

  Sequence s1;
  ExpectStorePolicy(s1);

  EXPECT_CALL(*key_, IsPopulated()).WillRepeatedly(Return(false));

  EXPECT_TRUE(service_->Store(policy_data_, policy_len_, completion_,
                              PolicyService::KEY_NONE,
                              SignatureCheck::kEnabled));
  fake_loop_.Run();
  EXPECT_FALSE(base::PathExists(key_copy_file_));
}

TEST_F(UserPolicyServiceTest, StoreInvalidSignature) {
  InitPolicy(em::PolicyData::ACTIVE, fake_signature_);

  InSequence s;
  EXPECT_CALL(*key_, Verify(_, _, _, _)).WillOnce(Return(false));

  EXPECT_FALSE(service_->Store(policy_data_, policy_len_,
                               MockPolicyService::CreateExpectFailureCallback(),
                               PolicyService::KEY_NONE,
                               SignatureCheck::kEnabled));

  fake_loop_.Run();
}

TEST_F(UserPolicyServiceTest, PersistKeyCopy) {
  std::vector<uint8_t> key_value;
  key_value.push_back(0x12);
  EXPECT_CALL(*key_, IsPopulated()).WillRepeatedly(Return(true));
  EXPECT_CALL(*key_, public_key_der()).WillOnce(ReturnRef(key_value));
  EXPECT_FALSE(base::PathExists(key_copy_file_));

  service_->PersistKeyCopy();
  EXPECT_TRUE(base::PathExists(key_copy_file_));
  std::string content;
  EXPECT_TRUE(base::ReadFileToString(key_copy_file_, &content));
  ASSERT_EQ(1u, content.size());
  EXPECT_EQ(key_value[0], content[0]);

  // Now persist an empty key, and verify that the copy is removed.
  EXPECT_CALL(*key_, IsPopulated()).WillRepeatedly(Return(false));
  service_->PersistKeyCopy();
  EXPECT_FALSE(base::PathExists(key_copy_file_));
}

}  // namespace login_manager
