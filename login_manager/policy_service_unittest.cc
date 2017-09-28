// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/policy_service.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include <base/run_loop.h>
#include <base/threading/thread.h>
#include <brillo/message_loops/fake_message_loop.h>
#include <chromeos/dbus/service_constants.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "bindings/device_management_backend.pb.h"
#include "login_manager/blob_util.h"
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
  PolicyServiceTest() = default;

  virtual void SetUp() {
    fake_loop_.SetAsCurrent();
    store_ = new StrictMock<MockPolicyStore>;
    service_ = std::make_unique<PolicyService>(
        std::unique_ptr<PolicyStore>(store_), &key_);
    service_->set_delegate(&delegate_);
  }

  void InitPolicy(const std::vector<uint8_t>& data,
                  const std::vector<uint8_t>& signature,
                  const std::vector<uint8_t>& key,
                  const std::vector<uint8_t>& key_signature) {
    policy_proto_.Clear();
    if (!data.empty())
      policy_proto_.set_policy_data(BlobToString(data));
    if (!signature.empty())
      policy_proto_.set_policy_data_signature(BlobToString(signature));
    if (!key.empty())
      policy_proto_.set_new_public_key(BlobToString(key));
    if (!key_signature.empty())
      policy_proto_.set_new_public_key_signature(BlobToString(key_signature));
  }

  void ExpectVerifyAndSetPolicy(Sequence* sequence) {
    EXPECT_CALL(key_, Verify(fake_data_, fake_sig_))
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
    EXPECT_CALL(*store_, Persist())
        .InSequence(*sequence)
        .WillOnce(Return(true));
    EXPECT_CALL(delegate_, OnPolicyPersisted(true));
  }

  void ExpectKeyEqualsFalse(Sequence* sequence) {
    EXPECT_CALL(key_, Equals(_))
        .InSequence(*sequence)
        .WillRepeatedly(Return(false));
  }

  void ExpectKeyPopulated(Sequence* sequence, bool return_value) {
    EXPECT_CALL(key_, IsPopulated())
        .InSequence(*sequence)
        .WillRepeatedly(Return(return_value));
  }

  void ExpectStoreFail(int flags,
                       SignatureCheck signature_check,
                       const std::string& code) {
    EXPECT_CALL(key_, Persist()).Times(0);
    EXPECT_CALL(*store_, Set(_)).Times(0);
    EXPECT_CALL(*store_, Persist()).Times(0);

    EXPECT_FALSE(
        service_->Store(SerializeAsBlob(policy_proto_), flags, signature_check,
                        MockPolicyService::CreateExpectFailureCallback()));
    fake_loop_.Run();
  }

  PolicyStore* store() { return service_->store(); }
  PolicyKey* key() { return service_->key(); }

  static const int kAllKeyFlags;
  static const char kSignalSuccess[];
  static const char kSignalFailure[];

  const std::vector<uint8_t> fake_data_ = StringToBlob("fake_data");
  const std::vector<uint8_t> fake_sig_ = StringToBlob("fake_signature");
  const std::vector<uint8_t> fake_key_ = StringToBlob("fake_key");
  const std::vector<uint8_t> fake_key_sig_ = StringToBlob("fake_key_signature");

  const std::vector<uint8_t> empty_blob_;

  // Various representations of the policy protobuf.
  em::PolicyFetchResponse policy_proto_;

  brillo::FakeMessageLoop fake_loop_{nullptr};

  // Use StrictMock to make sure that no unexpected policy or key mutations can
  // occur without the test failing.
  StrictMock<MockPolicyKey> key_;
  StrictMock<MockPolicyStore>* store_;
  MockPolicyServiceDelegate delegate_;

  std::unique_ptr<PolicyService> service_;
};

const int PolicyServiceTest::kAllKeyFlags = PolicyService::KEY_ROTATE |
                                            PolicyService::KEY_INSTALL_NEW |
                                            PolicyService::KEY_CLOBBER;
const char PolicyServiceTest::kSignalSuccess[] = "success";
const char PolicyServiceTest::kSignalFailure[] = "failure";

TEST_F(PolicyServiceTest, Store) {
  InitPolicy(fake_data_, fake_sig_, empty_blob_, empty_blob_);

  Sequence s1, s2;
  ExpectKeyEqualsFalse(&s1);
  ExpectKeyPopulated(&s2, true);
  EXPECT_CALL(key_, Verify(fake_data_, fake_sig_))
      .InSequence(s1, s2)
      .WillRepeatedly(Return(true));
  ExpectKeyPopulated(&s1, true);
  ExpectVerifyAndSetPolicy(&s2);
  ExpectPersistPolicy(&s2);

  EXPECT_TRUE(service_->Store(
      SerializeAsBlob(policy_proto_), kAllKeyFlags, SignatureCheck::kEnabled,
      MockPolicyService::CreateExpectSuccessCallback()));

  fake_loop_.Run();
}

TEST_F(PolicyServiceTest, StoreUnsigned) {
  InitPolicy(fake_data_, empty_blob_, empty_blob_, empty_blob_);

  Sequence s1, s2;
  ExpectSetPolicy(&s1);
  ExpectPersistPolicy(&s2);

  EXPECT_TRUE(service_->Store(
      SerializeAsBlob(policy_proto_), kAllKeyFlags, SignatureCheck::kDisabled,
      MockPolicyService::CreateExpectSuccessCallback()));

  fake_loop_.Run();
}

TEST_F(PolicyServiceTest, StoreWrongSignature) {
  InitPolicy(fake_data_, fake_sig_, empty_blob_, empty_blob_);

  Sequence s1, s2;
  ExpectKeyEqualsFalse(&s1);
  ExpectKeyPopulated(&s2, true);
  EXPECT_CALL(key_, Verify(fake_data_, fake_sig_))
      .InSequence(s1, s2)
      .WillRepeatedly(Return(false));

  ExpectStoreFail(kAllKeyFlags, SignatureCheck::kEnabled,
                  dbus_error::kVerifyFail);
}

TEST_F(PolicyServiceTest, StoreNoData) {
  InitPolicy(empty_blob_, empty_blob_, empty_blob_, empty_blob_);

  ExpectStoreFail(kAllKeyFlags, SignatureCheck::kEnabled,
                  dbus_error::kSigDecodeFail);
}

TEST_F(PolicyServiceTest, StoreNoSignature) {
  InitPolicy(fake_data_, empty_blob_, empty_blob_, empty_blob_);

  EXPECT_CALL(key_, Verify(fake_data_, std::vector<uint8_t>()))
      .WillOnce(Return(false));

  ExpectStoreFail(kAllKeyFlags, SignatureCheck::kEnabled,
                  dbus_error::kVerifyFail);
}

TEST_F(PolicyServiceTest, StoreNoKey) {
  InitPolicy(fake_data_, fake_sig_, empty_blob_, empty_blob_);

  Sequence s1, s2;
  ExpectKeyEqualsFalse(&s1);
  ExpectKeyPopulated(&s2, false);
  EXPECT_CALL(key_, Verify(fake_data_, fake_sig_))
      .InSequence(s1, s2)
      .WillRepeatedly(Return(false));

  ExpectStoreFail(kAllKeyFlags, SignatureCheck::kEnabled,
                  dbus_error::kVerifyFail);
}

TEST_F(PolicyServiceTest, StoreNewKey) {
  InitPolicy(fake_data_, fake_sig_, fake_key_, empty_blob_);

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

  EXPECT_TRUE(service_->Store(
      SerializeAsBlob(policy_proto_), kAllKeyFlags, SignatureCheck::kEnabled,
      MockPolicyService::CreateExpectSuccessCallback()));

  fake_loop_.Run();
}

TEST_F(PolicyServiceTest, StoreNewKeyClobber) {
  InitPolicy(fake_data_, fake_sig_, fake_key_, empty_blob_);

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

  EXPECT_TRUE(
      service_->Store(SerializeAsBlob(policy_proto_),
                      PolicyService::KEY_CLOBBER, SignatureCheck::kEnabled,
                      MockPolicyService::CreateExpectSuccessCallback()));

  fake_loop_.Run();
}

TEST_F(PolicyServiceTest, StoreNewKeySame) {
  InitPolicy(fake_data_, fake_sig_, fake_key_, empty_blob_);

  Sequence s1, s2, s3;
  EXPECT_CALL(key_, Equals(BlobToString(fake_key_)))
      .InSequence(s1)
      .WillRepeatedly(Return(true));
  ExpectKeyPopulated(&s2, true);
  ExpectVerifyAndSetPolicy(&s3);
  ExpectPersistPolicy(&s2);

  EXPECT_TRUE(service_->Store(
      SerializeAsBlob(policy_proto_), kAllKeyFlags, SignatureCheck::kEnabled,
      MockPolicyService::CreateExpectSuccessCallback()));

  fake_loop_.Run();
}

TEST_F(PolicyServiceTest, StoreNewKeyNotAllowed) {
  InitPolicy(fake_data_, fake_sig_, fake_key_, empty_blob_);

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

  EXPECT_TRUE(service_->Store(
      SerializeAsBlob(policy_proto_), kAllKeyFlags, SignatureCheck::kEnabled,
      MockPolicyService::CreateExpectSuccessCallback()));

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

  EXPECT_TRUE(
      service_->Store(SerializeAsBlob(policy_proto_),
                      PolicyService::KEY_CLOBBER, SignatureCheck::kEnabled,
                      MockPolicyService::CreateExpectSuccessCallback()));

  fake_loop_.Run();
}

TEST_F(PolicyServiceTest, StoreRotationNoSignature) {
  InitPolicy(fake_data_, fake_sig_, fake_key_, empty_blob_);

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

  std::vector<uint8_t> out_policy_blob;
  EXPECT_TRUE(service_->Retrieve(&out_policy_blob));
  EXPECT_EQ(SerializeAsBlob(policy_proto_), out_policy_blob);
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
