// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/policy_service.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include <base/memory/ptr_util.h>
#include <base/run_loop.h>
#include <base/threading/thread.h>
#include <brillo/message_loops/fake_message_loop.h>
#include <chromeos/dbus/service_constants.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "bindings/device_management_backend.pb.h"
#include "login_manager/matchers.h"
#include "login_manager/mock_policy_key.h"
#include "login_manager/mock_policy_service.h"
#include "login_manager/mock_policy_store.h"

namespace em = enterprise_management;

using ::testing::DoAll;
using ::testing::InvokeWithoutArgs;
using ::testing::Mock;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::Sequence;
using ::testing::SetArgumentPointee;
using ::testing::StrictMock;
using ::testing::_;

namespace login_manager {

class PolicyServiceTest : public testing::Test {
 public:
  PolicyServiceTest()
      : fake_data_("fake_data"),
        fake_sig_("fake_signature"),
        fake_key_("fake_key"),
        fake_key_sig_("fake_key_signature") {
  }

  virtual void SetUp() {
    fake_loop_.SetAsCurrent();
    store_ = new StrictMock<MockPolicyStore>;
    service_ = base::MakeUnique<PolicyService>(
        std::unique_ptr<PolicyStore>(store_), &key_);
    service_->set_delegate(&delegate_);
  }

  void InitPolicy(const std::string& data,
                  const std::string& signature,
                  const std::string& key,
                  const std::string& key_signature) {
    policy_proto_.Clear();
    if (!data.empty())
      policy_proto_.set_policy_data(data);
    if (!signature.empty())
      policy_proto_.set_policy_data_signature(signature);
    if (!key.empty())
      policy_proto_.set_new_public_key(key);
    if (!key_signature.empty())
      policy_proto_.set_new_public_key_signature(key_signature);

    ASSERT_TRUE(policy_proto_.SerializeToString(&policy_str_));
    policy_data_ = reinterpret_cast<const uint8_t*>(policy_str_.c_str());
    policy_len_ = policy_str_.size();
  }

  void ExpectVerifyAndSetPolicy(Sequence* sequence) {
    EXPECT_CALL(key_,
                Verify(CastEq(fake_data_),
                       fake_data_.size(),
                       CastEq(fake_sig_),
                       fake_sig_.size()))
        .InSequence(*sequence)
        .WillOnce(Return(true));
    EXPECT_CALL(*store_, Set(ProtoEq(policy_proto_)))
        .Times(1)
        .InSequence(*sequence);
  }

  void ExpectSetPolicy(Sequence* sequence) {
    EXPECT_CALL(*store_, Set(ProtoEq(policy_proto_)))
        .Times(1)
        .InSequence(*sequence);
  }

  void ExpectPersistKey(Sequence* sequence) {
    EXPECT_CALL(key_, Persist()).InSequence(*sequence).WillOnce(Return(true));
    EXPECT_CALL(delegate_, OnKeyPersisted(true));
  }

  void ExpectPersistPolicy(Sequence* sequence) {
    EXPECT_CALL(*store_, Persist()).InSequence(*sequence).WillOnce(
        Return(true));
    EXPECT_CALL(delegate_, OnPolicyPersisted(true));
    completion_ = MockPolicyService::CreateExpectSuccessCallback();
  }

  void ExpectKeyEqualsFalse(Sequence* sequence) {
    EXPECT_CALL(key_, Equals(_)).InSequence(*sequence).WillRepeatedly(
        Return(false));
  }

  void ExpectKeyPopulated(Sequence* sequence, bool return_value) {
    EXPECT_CALL(key_, IsPopulated()).InSequence(*sequence).WillRepeatedly(
        Return(return_value));
  }

  void ExpectStoreFail(int flags,
                       SignatureCheck signature_check,
                       const std::string& code) {
    EXPECT_CALL(key_, Persist()).Times(0);
    EXPECT_CALL(*store_, Set(_)).Times(0);
    EXPECT_CALL(*store_, Persist()).Times(0);

    EXPECT_FALSE(
        service_->Store(policy_data_, policy_len_,
                        MockPolicyService::CreateExpectFailureCallback(), flags,
                        signature_check));
    fake_loop_.Run();
  }

  PolicyStore* store() { return service_->store(); }
  PolicyKey* key() { return service_->key(); }

  static const int kAllKeyFlags;
  static const char kSignalSuccess[];
  static const char kSignalFailure[];

  std::string fake_data_;
  std::string fake_sig_;
  std::string fake_key_;
  std::string fake_key_sig_;

  // Various representations of the policy protobuf.
  em::PolicyFetchResponse policy_proto_;
  std::string policy_str_;
  const uint8_t* policy_data_;
  uint32_t policy_len_;

  brillo::FakeMessageLoop fake_loop_{nullptr};

  // Use StrictMock to make sure that no unexpected policy or key mutations can
  // occur without the test failing.
  StrictMock<MockPolicyKey> key_;
  StrictMock<MockPolicyStore>* store_;
  MockPolicyServiceDelegate delegate_;
  PolicyService::Completion completion_;

  std::unique_ptr<PolicyService> service_;
};

const int PolicyServiceTest::kAllKeyFlags = PolicyService::KEY_ROTATE |
                                            PolicyService::KEY_INSTALL_NEW |
                                            PolicyService::KEY_CLOBBER;
const char PolicyServiceTest::kSignalSuccess[] = "success";
const char PolicyServiceTest::kSignalFailure[] = "failure";

TEST_F(PolicyServiceTest, Store) {
  InitPolicy(fake_data_, fake_sig_, "", "");

  Sequence s1, s2;
  ExpectKeyEqualsFalse(&s1);
  ExpectKeyPopulated(&s2, true);
  EXPECT_CALL(key_,
              Verify(CastEq(fake_data_),
                     fake_data_.size(),
                     CastEq(fake_sig_),
                     fake_sig_.size()))
      .InSequence(s1, s2)
      .WillRepeatedly(Return(true));
  ExpectKeyPopulated(&s1, true);
  ExpectVerifyAndSetPolicy(&s2);
  ExpectPersistPolicy(&s2);

  EXPECT_TRUE(service_->Store(policy_data_, policy_len_, completion_,
                              kAllKeyFlags, SignatureCheck::kEnabled));

  fake_loop_.Run();
}

TEST_F(PolicyServiceTest, StoreUnsigned) {
  InitPolicy(fake_data_, "", "", "");

  Sequence s1, s2;
  ExpectSetPolicy(&s1);
  ExpectPersistPolicy(&s2);

  EXPECT_TRUE(service_->Store(policy_data_, policy_len_, completion_,
                              kAllKeyFlags, SignatureCheck::kDisabled));

  fake_loop_.Run();
}

TEST_F(PolicyServiceTest, StoreWrongSignature) {
  InitPolicy(fake_data_, fake_sig_, "", "");

  Sequence s1, s2;
  ExpectKeyEqualsFalse(&s1);
  ExpectKeyPopulated(&s2, true);
  EXPECT_CALL(key_,
              Verify(CastEq(fake_data_),
                     fake_data_.size(),
                     CastEq(fake_sig_),
                     fake_sig_.size()))
      .InSequence(s1, s2)
      .WillRepeatedly(Return(false));

  ExpectStoreFail(kAllKeyFlags, SignatureCheck::kEnabled,
                  dbus_error::kVerifyFail);
}

TEST_F(PolicyServiceTest, StoreNoData) {
  InitPolicy("", "", "", "");

  ExpectStoreFail(kAllKeyFlags, SignatureCheck::kEnabled,
                  dbus_error::kSigDecodeFail);
}

TEST_F(PolicyServiceTest, StoreNoSignature) {
  InitPolicy(fake_data_, "", "", "");

  EXPECT_CALL(key_,
              Verify(CastEq(fake_data_),
                     fake_data_.size(),
                     CastEq(std::string()),
                     0))
      .WillOnce(Return(false));

  ExpectStoreFail(kAllKeyFlags, SignatureCheck::kEnabled,
                  dbus_error::kVerifyFail);
}

TEST_F(PolicyServiceTest, StoreNoKey) {
  InitPolicy(fake_data_, fake_sig_, "", "");

  Sequence s1, s2;
  ExpectKeyEqualsFalse(&s1);
  ExpectKeyPopulated(&s2, false);
  EXPECT_CALL(key_,
              Verify(CastEq(fake_data_),
                     fake_data_.size(),
                     CastEq(fake_sig_),
                     fake_sig_.size()))
      .InSequence(s1, s2)
      .WillRepeatedly(Return(false));

  ExpectStoreFail(kAllKeyFlags, SignatureCheck::kEnabled,
                  dbus_error::kVerifyFail);
}

TEST_F(PolicyServiceTest, StoreNewKey) {
  InitPolicy(fake_data_, fake_sig_, fake_key_, "");

  Sequence s1, s2;
  ExpectKeyEqualsFalse(&s1);
  ExpectKeyPopulated(&s2, false);
  EXPECT_CALL(key_, PopulateFromBuffer(VectorEq(fake_key_)))
      .InSequence(s1, s2)
      .WillOnce(Return(true));
  ExpectKeyPopulated(&s1, true);
  ExpectVerifyAndSetPolicy(&s2);
  ExpectPersistKey(&s1);
  ExpectPersistPolicy(&s2);

  EXPECT_TRUE(service_->Store(policy_data_, policy_len_, completion_,
                              kAllKeyFlags, SignatureCheck::kEnabled));

  fake_loop_.Run();
}

TEST_F(PolicyServiceTest, StoreNewKeyClobber) {
  InitPolicy(fake_data_, fake_sig_, fake_key_, "");

  Sequence s1, s2;
  ExpectKeyEqualsFalse(&s1);
  ExpectKeyPopulated(&s2, false);
  EXPECT_CALL(key_, ClobberCompromisedKey(VectorEq(fake_key_)))
      .InSequence(s1, s2)
      .WillOnce(Return(true));
  ExpectKeyPopulated(&s1, true);
  ExpectVerifyAndSetPolicy(&s2);
  ExpectPersistKey(&s1);
  ExpectPersistPolicy(&s2);

  EXPECT_TRUE(service_->Store(policy_data_, policy_len_, completion_,
                              PolicyService::KEY_CLOBBER,
                              SignatureCheck::kEnabled));

  fake_loop_.Run();
}

TEST_F(PolicyServiceTest, StoreNewKeySame) {
  InitPolicy(fake_data_, fake_sig_, fake_key_, "");

  Sequence s1, s2, s3;
  EXPECT_CALL(key_, Equals(fake_key_)).InSequence(s1).WillRepeatedly(
      Return(true));
  ExpectKeyPopulated(&s2, true);
  ExpectVerifyAndSetPolicy(&s3);
  ExpectPersistPolicy(&s2);

  EXPECT_TRUE(service_->Store(policy_data_, policy_len_, completion_,
                              kAllKeyFlags, SignatureCheck::kEnabled));

  fake_loop_.Run();
}

TEST_F(PolicyServiceTest, StoreNewKeyNotAllowed) {
  InitPolicy(fake_data_, fake_sig_, fake_key_, "");

  Sequence s1, s2;
  ExpectKeyEqualsFalse(&s1);
  ExpectKeyPopulated(&s2, false);

  ExpectStoreFail(PolicyService::KEY_NONE, SignatureCheck::kEnabled,
                  dbus_error::kPubkeySetIllegal);
}

TEST_F(PolicyServiceTest, StoreRotation) {
  InitPolicy(fake_data_, fake_sig_, fake_key_, fake_key_sig_);

  Sequence s1, s2;
  ExpectKeyEqualsFalse(&s1);
  ExpectKeyPopulated(&s2, true);
  EXPECT_CALL(key_, Rotate(VectorEq(fake_key_), VectorEq(fake_key_sig_)))
      .InSequence(s1, s2)
      .WillOnce(Return(true));
  ExpectKeyPopulated(&s1, true);
  ExpectVerifyAndSetPolicy(&s2);
  ExpectPersistKey(&s1);
  ExpectPersistPolicy(&s2);

  EXPECT_TRUE(service_->Store(policy_data_, policy_len_, completion_,
                              kAllKeyFlags, SignatureCheck::kEnabled));

  fake_loop_.Run();
}

TEST_F(PolicyServiceTest, StoreRotationClobber) {
  InitPolicy(fake_data_, fake_sig_, fake_key_, fake_key_sig_);

  Sequence s1, s2;
  ExpectKeyEqualsFalse(&s1);
  ExpectKeyPopulated(&s2, false);
  EXPECT_CALL(key_, ClobberCompromisedKey(VectorEq(fake_key_)))
      .InSequence(s1, s2)
      .WillOnce(Return(true));
  ExpectKeyPopulated(&s1, true);
  ExpectVerifyAndSetPolicy(&s2);
  ExpectPersistKey(&s1);
  ExpectPersistPolicy(&s2);

  EXPECT_TRUE(service_->Store(policy_data_, policy_len_, completion_,
                              PolicyService::KEY_CLOBBER,
                              SignatureCheck::kEnabled));

  fake_loop_.Run();
}

TEST_F(PolicyServiceTest, StoreRotationNoSignature) {
  InitPolicy(fake_data_, fake_sig_, fake_key_, "");

  Sequence s1, s2;
  ExpectKeyEqualsFalse(&s1);
  ExpectKeyPopulated(&s2, true);

  ExpectStoreFail(PolicyService::KEY_ROTATE, SignatureCheck::kEnabled,
                  dbus_error::kPubkeySetIllegal);
}

TEST_F(PolicyServiceTest, StoreRotationBadSignature) {
  InitPolicy(fake_data_, fake_sig_, fake_key_, fake_key_sig_);

  Sequence s1, s2;
  ExpectKeyEqualsFalse(&s1);
  ExpectKeyPopulated(&s2, true);
  EXPECT_CALL(key_, Rotate(VectorEq(fake_key_), VectorEq(fake_key_sig_)))
      .InSequence(s1, s2)
      .WillOnce(Return(false));

  ExpectStoreFail(PolicyService::KEY_ROTATE, SignatureCheck::kEnabled,
                  dbus_error::kPubkeySetIllegal);
}

TEST_F(PolicyServiceTest, StoreRotationNotAllowed) {
  InitPolicy(fake_data_, fake_sig_, fake_key_, fake_key_sig_);

  Sequence s1, s2;
  ExpectKeyEqualsFalse(&s1);
  ExpectKeyPopulated(&s2, true);

  ExpectStoreFail(PolicyService::KEY_NONE, SignatureCheck::kEnabled,
                  dbus_error::kPubkeySetIllegal);
}

TEST_F(PolicyServiceTest, Retrieve) {
  InitPolicy(fake_data_, fake_sig_, fake_key_, fake_key_sig_);

  EXPECT_CALL(*store_, Get()).WillOnce(ReturnRef(policy_proto_));

  std::vector<uint8_t> policy_data;
  EXPECT_TRUE(service_->Retrieve(&policy_data));
  ASSERT_EQ(policy_str_.size(), policy_data.size());
  EXPECT_TRUE(
      std::equal(policy_str_.begin(), policy_str_.end(), policy_data.begin()));
}

TEST_F(PolicyServiceTest, PersistPolicySuccess) {
  EXPECT_CALL(*store_, Persist()).WillOnce(Return(true));
  EXPECT_CALL(delegate_, OnPolicyPersisted(true)).Times(1);
  service_->PersistPolicy(PolicyService::Completion());
}

TEST_F(PolicyServiceTest, PersistPolicyFailure) {
  EXPECT_CALL(*store_, Persist()).WillOnce(Return(false));
  EXPECT_CALL(delegate_, OnPolicyPersisted(false)).Times(1);
  service_->PersistPolicy(PolicyService::Completion());
}

}  // namespace login_manager
