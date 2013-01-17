// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "user_policy_service.h"

#include <vector>

#include <base/basictypes.h>
#include <base/file_util.h>
#include <base/message_loop.h>
#include <base/message_loop_proxy.h>
#include <base/scoped_temp_dir.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "login_manager/device_management_backend.pb.h"
#include "login_manager/matchers.h"
#include "login_manager/mock_policy_key.h"
#include "login_manager/mock_policy_service.h"
#include "login_manager/mock_policy_store.h"
#include "login_manager/system_utils.h"

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
      : fake_signature_("fake_signature"),
        policy_data_(NULL),
        policy_len_(0) {
  }

  virtual void SetUp() {
    ASSERT_TRUE(tmpdir_.CreateUniqueTempDir());
    key_copy_file_ = tmpdir_.path().Append("key_copy.pub");

    key_ = new StrictMock<MockPolicyKey>;
    store_ = new StrictMock<MockPolicyStore>;
    scoped_refptr<base::MessageLoopProxy> message_loop(
        base::MessageLoopProxy::current());
    service_ = new UserPolicyService(scoped_ptr<PolicyStore>(store_),
                                     scoped_ptr<PolicyKey>(key_),
                                     key_copy_file_,
                                     message_loop,
                                     &system_utils_);
  }

  void InitPolicy(em::PolicyData::AssociationState state,
                  const std::string& signature) {
    em::PolicyData policy_data;
    policy_data.set_state(state);

    policy_proto_.Clear();
    ASSERT_TRUE(policy_data.SerializeToString(
        policy_proto_.mutable_policy_data()));
    if (!signature.empty())
      policy_proto_.set_policy_data_signature(signature);

    ASSERT_TRUE(policy_proto_.SerializeToString(&policy_str_));
    policy_data_ = reinterpret_cast<const uint8*>(policy_str_.c_str());
    policy_len_ = policy_str_.size();
  }

  void ExpectStorePolicy(const Sequence& sequence) {
    EXPECT_CALL(*store_, Set(PolicyStrEq(policy_str_)))
        .InSequence(sequence);
    EXPECT_CALL(*store_, Persist())
        .InSequence(sequence)
        .WillOnce(Return(true));
    EXPECT_CALL(completion_, Success())
        .InSequence(sequence);
  }

 protected:
  SystemUtils system_utils_;
  ScopedTempDir tmpdir_;
  FilePath key_copy_file_;

  const std::string fake_signature_;

  // Various representations of the policy protobuf.
  em::PolicyFetchResponse policy_proto_;
  std::string policy_str_;
  const uint8* policy_data_;
  uint32 policy_len_;

  MessageLoop loop_;

  // Use StrictMock to make sure that no unexpected policy or key mutations can
  // occur without the test failing.
  StrictMock<MockPolicyKey>* key_;
  StrictMock<MockPolicyStore>* store_;
  MockPolicyServiceCompletion completion_;

  scoped_refptr<UserPolicyService> service_;

 private:
  DISALLOW_COPY_AND_ASSIGN(UserPolicyServiceTest);
};

TEST_F(UserPolicyServiceTest, StoreSignedPolicy) {
  InitPolicy(em::PolicyData::ACTIVE, fake_signature_);

  Sequence s1;
  EXPECT_CALL(*key_, Verify(_, _, _, _))
      .InSequence(s1)
      .WillOnce(Return(true));
  ExpectStorePolicy(s1);

  EXPECT_TRUE(service_->Store(policy_data_, policy_len_, &completion_, 0));
  loop_.RunAllPending();
}

TEST_F(UserPolicyServiceTest, StoreUnmanagedSigned) {
  InitPolicy(em::PolicyData::UNMANAGED, fake_signature_);

  Sequence s1;
  EXPECT_CALL(*key_, Verify(_, _, _, _))
      .InSequence(s1)
      .WillOnce(Return(true));
  ExpectStorePolicy(s1);

  EXPECT_TRUE(service_->Store(policy_data_, policy_len_, &completion_, 0));
  loop_.RunAllPending();
}

TEST_F(UserPolicyServiceTest, StoreUnmanagedKeyPresent) {
  InitPolicy(em::PolicyData::UNMANAGED, "");

  Sequence s1;
  ExpectStorePolicy(s1);
  std::vector<uint8> key_value;
  key_value.push_back(0x12);

  EXPECT_CALL(*key_, IsPopulated())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*key_, public_key_der())
      .WillRepeatedly(ReturnRef(key_value));

  Sequence s2;
  EXPECT_CALL(*key_, ClobberCompromisedKey(ElementsAre()))
      .InSequence(s2);
  EXPECT_CALL(*key_, Persist())
      .InSequence(s2)
      .WillOnce(Return(true));

  EXPECT_FALSE(file_util::PathExists(key_copy_file_));
  EXPECT_TRUE(service_->Store(policy_data_, policy_len_, &completion_, 0));
  loop_.RunAllPending();

  EXPECT_TRUE(file_util::PathExists(key_copy_file_));
  std::string content;
  EXPECT_TRUE(file_util::ReadFileToString(key_copy_file_, &content));
  ASSERT_EQ(1u, content.size());
  EXPECT_EQ(key_value[0], content[0]);
}

TEST_F(UserPolicyServiceTest, StoreUnmanagedNoKey) {
  InitPolicy(em::PolicyData::UNMANAGED, "");

  Sequence s1;
  ExpectStorePolicy(s1);

  EXPECT_CALL(*key_, IsPopulated())
      .WillRepeatedly(Return(false));

  EXPECT_TRUE(service_->Store(policy_data_, policy_len_, &completion_, 0));
  loop_.RunAllPending();
  EXPECT_FALSE(file_util::PathExists(key_copy_file_));
}

TEST_F(UserPolicyServiceTest, StoreInvalidSignature) {
  InitPolicy(em::PolicyData::ACTIVE, fake_signature_);

  InSequence s;
  EXPECT_CALL(*key_, Verify(_, _, _, _))
      .WillOnce(Return(false));
  EXPECT_CALL(completion_, Failure(_));

  EXPECT_FALSE(service_->Store(policy_data_, policy_len_, &completion_, 0));
  loop_.RunAllPending();
}

TEST_F(UserPolicyServiceTest, PersistKeyCopy) {
  std::vector<uint8> key_value;
  key_value.push_back(0x12);
  EXPECT_CALL(*key_, IsPopulated())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*key_, public_key_der())
      .WillOnce(ReturnRef(key_value));
  EXPECT_FALSE(file_util::PathExists(key_copy_file_));

  service_->PersistKeyCopy();
  EXPECT_TRUE(file_util::PathExists(key_copy_file_));
  std::string content;
  EXPECT_TRUE(file_util::ReadFileToString(key_copy_file_, &content));
  ASSERT_EQ(1u, content.size());
  EXPECT_EQ(key_value[0], content[0]);

  // Now persist an empty key, and verify that the copy is removed.
  EXPECT_CALL(*key_, IsPopulated())
      .WillRepeatedly(Return(false));
  service_->PersistKeyCopy();
  EXPECT_FALSE(file_util::PathExists(key_copy_file_));
}

}  // namespace login_manager
