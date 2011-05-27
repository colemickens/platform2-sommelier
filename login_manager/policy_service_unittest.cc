// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "policy_service.h"

#include <algorithm>
#include <string>
#include <vector>

#include <dbus/dbus-glib-lowlevel.h>

#include <base/message_loop.h>
#include <base/message_loop_proxy.h>
#include <base/threading/thread.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "login_manager/bindings/device_management_backend.pb.h"
#include "login_manager/mock_owner_key.h"
#include "login_manager/mock_policy_store.h"
#include "login_manager/mock_system_utils.h"

namespace em = enterprise_management;

using ::testing::DoAll;
using ::testing::Mock;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::Sequence;
using ::testing::SetArgumentPointee;
using ::testing::StrictMock;
using ::testing::_;

MATCHER_P(CastEq, str, "") {
  return std::string(reinterpret_cast<const char*>(arg)) == str;
}

MATCHER_P(VectorEq, str, "") {
  return str.size() == arg.size() &&
      std::equal(str.begin(), str.end(), arg.begin());
}

MATCHER_P(PolicyEq, str, "") {
  std::string arg_policy;
  return arg.SerializeToString(&arg_policy) && arg_policy == str;
}

namespace login_manager {

class PolicyServiceTest : public testing::Test {
 public:
  PolicyServiceTest()
      : fake_data_("fake_data"),
        fake_sig_("fake_signature"),
        fake_key_("fake_key"),
        fake_key_sig_("fake_key_signature"),
        policy_blob_(NULL) {
  }

  virtual void SetUp() {
    key_ = new StrictMock<MockOwnerKey>;
    store_ = new StrictMock<MockPolicyStore>;
    system_ = new MockSystemUtils;
    scoped_refptr<base::MessageLoopProxy> message_loop(
        base::MessageLoopProxy::CreateForCurrentThread());
    service_ = new PolicyService(store_, key_, system_,
                                 message_loop, message_loop);
  }

  virtual void TearDown() {
    if (policy_blob_)
      g_array_free(policy_blob_, TRUE);
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
    policy_blob_ =
        g_array_sized_new(FALSE, FALSE, sizeof(char), policy_str_.size());
    ASSERT_TRUE(policy_blob_);
    g_array_append_vals(policy_blob_, policy_str_.c_str(), policy_str_.size());
  }

  void ExpectVerifyAndSetPolicy(Sequence& sequence) {
    EXPECT_CALL(*key_, Verify(CastEq(fake_data_), fake_data_.size(),
                              CastEq(fake_sig_), fake_sig_.size()))
        .InSequence(sequence)
        .WillOnce(Return(true));
    EXPECT_CALL(*store_, Set(PolicyEq(policy_str_))).Times(1)
        .InSequence(sequence);
  }

  void ExpectPersistKey(Sequence& sequence) {
    EXPECT_CALL(*key_, Persist())
        .InSequence(sequence)
        .WillOnce(Return(true));
  }

  void ExpectPersistPolicy(Sequence& sequence) {
    EXPECT_CALL(*store_, Persist())
        .InSequence(sequence)
        .WillOnce(Return(true));
  }

  void ExpectKeyEqualsFalse(Sequence& sequence) {
    EXPECT_CALL(*key_, Equals(_))
        .InSequence(sequence)
        .WillRepeatedly(Return(false));
  }

  void ExpectKeyPopulated(Sequence& sequence, bool return_value) {
    EXPECT_CALL(*key_, IsPopulated())
        .InSequence(sequence)
        .WillRepeatedly(Return(return_value));
  }

  void ExpectStoreFail(int flags, ChromeOSLoginError code) {
    EXPECT_CALL(*key_, Persist()).Times(0);
    EXPECT_CALL(*store_, Set(_)).Times(0);
    EXPECT_CALL(*store_, Persist()).Times(0);
    EXPECT_CALL(*system_, SetAndSendGError(code, _, _)).Times(1);

    EXPECT_EQ(FALSE, service_->Store(policy_blob_, NULL, flags));
    loop_.RunAllPending();
  }

  PolicyStore* store() { return service_->store(); }
  OwnerKey* key() { return service_->key(); }

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
  GArray* policy_blob_;

  MessageLoop loop_;

  // Use StrictMock to make sure that no unexpected policy or key mutations can
  // occur without the test failing.
  StrictMock<MockOwnerKey>* key_;
  StrictMock<MockPolicyStore>* store_;
  MockSystemUtils* system_;

  scoped_refptr<PolicyService> service_;
};

const int PolicyServiceTest::kAllKeyFlags = PolicyService::KEY_ROTATE |
                                            PolicyService::KEY_INSTALL_NEW |
                                            PolicyService::KEY_CLOBBER;
const char PolicyServiceTest::kSignalSuccess[] = "success";
const char PolicyServiceTest::kSignalFailure[] = "failure";

TEST_F(PolicyServiceTest, Initialize) {
  EXPECT_CALL(*key_, PopulateFromDiskIfPossible())
      .WillOnce(Return(true));
  EXPECT_CALL(*key_, IsPopulated())
      .WillOnce(Return(true));
  EXPECT_CALL(*store_, LoadOrCreate())
      .WillOnce(Return(true));
  EXPECT_TRUE(service_->Initialize());
}

TEST_F(PolicyServiceTest, Store) {
  InitPolicy(fake_data_, fake_sig_, "", "");

  Sequence s1, s2;
  ExpectKeyEqualsFalse(s1);
  ExpectKeyPopulated(s2, true);
  EXPECT_CALL(*key_, Verify(CastEq(fake_data_), fake_data_.size(),
                            CastEq(fake_sig_), fake_sig_.size()))
      .InSequence(s1, s2)
      .WillRepeatedly(Return(true));
  ExpectKeyPopulated(s1, true);
  ExpectVerifyAndSetPolicy(s2);

  EXPECT_EQ(TRUE, service_->Store(policy_blob_, NULL, kAllKeyFlags));

  Mock::VerifyAndClearExpectations(key_);
  Mock::VerifyAndClearExpectations(store_);

  ExpectPersistPolicy(s2);
  loop_.RunAllPending();
}

TEST_F(PolicyServiceTest, StoreWrongSignature) {
  InitPolicy(fake_data_, fake_sig_, "", "");

  Sequence s1, s2;
  ExpectKeyEqualsFalse(s1);
  ExpectKeyPopulated(s2, true);
  EXPECT_CALL(*key_, Verify(CastEq(fake_data_), fake_data_.size(),
                            CastEq(fake_sig_), fake_sig_.size()))
      .InSequence(s1, s2)
      .WillRepeatedly(Return(false));

  ExpectStoreFail(kAllKeyFlags, CHROMEOS_LOGIN_ERROR_VERIFY_FAIL);
}

TEST_F(PolicyServiceTest, StoreNoData) {
  InitPolicy("", "", "", "");

  ExpectStoreFail(kAllKeyFlags, CHROMEOS_LOGIN_ERROR_DECODE_FAIL);
}

TEST_F(PolicyServiceTest, StoreNoSignature) {
  InitPolicy(fake_data_, "", "", "");

  ExpectStoreFail(kAllKeyFlags, CHROMEOS_LOGIN_ERROR_DECODE_FAIL);
}

TEST_F(PolicyServiceTest, StoreNoKey) {
  InitPolicy(fake_data_, fake_sig_, "", "");

  Sequence s1, s2;
  ExpectKeyEqualsFalse(s1);
  ExpectKeyPopulated(s2, false);
  EXPECT_CALL(*key_, Verify(CastEq(fake_data_), fake_data_.size(),
                            CastEq(fake_sig_), fake_sig_.size()))
      .InSequence(s1, s2)
      .WillRepeatedly(Return(false));

  ExpectStoreFail(kAllKeyFlags, CHROMEOS_LOGIN_ERROR_VERIFY_FAIL);
}

TEST_F(PolicyServiceTest, StoreNewKey) {
  InitPolicy(fake_data_, fake_sig_, fake_key_, "");

  Sequence s1, s2;
  ExpectKeyEqualsFalse(s1);
  ExpectKeyPopulated(s2, false);
  EXPECT_CALL(*key_, PopulateFromBuffer(VectorEq(fake_key_)))
      .InSequence(s1, s2)
      .WillOnce(Return(true));
  ExpectKeyPopulated(s1, true);
  ExpectVerifyAndSetPolicy(s2);

  EXPECT_EQ(TRUE, service_->Store(policy_blob_, NULL, kAllKeyFlags));

  Mock::VerifyAndClearExpectations(key_);
  Mock::VerifyAndClearExpectations(store_);

  ExpectPersistKey(s1);
  ExpectPersistPolicy(s2);
  loop_.RunAllPending();
}

TEST_F(PolicyServiceTest, StoreNewKeyClobber) {
  InitPolicy(fake_data_, fake_sig_, fake_key_, "");

  Sequence s1, s2;
  ExpectKeyEqualsFalse(s1);
  ExpectKeyPopulated(s2, false);
  EXPECT_CALL(*key_, ClobberCompromisedKey(VectorEq(fake_key_)))
      .InSequence(s1, s2)
      .WillOnce(Return(true));
  ExpectKeyPopulated(s1, true);
  ExpectVerifyAndSetPolicy(s2);

  EXPECT_EQ(TRUE, service_->Store(policy_blob_, NULL,
                                  PolicyService::KEY_CLOBBER));

  Mock::VerifyAndClearExpectations(key_);
  Mock::VerifyAndClearExpectations(store_);

  ExpectPersistKey(s1);
  ExpectPersistPolicy(s2);
  loop_.RunAllPending();
}

TEST_F(PolicyServiceTest, StoreNewKeySame) {
  InitPolicy(fake_data_, fake_sig_, fake_key_, "");

  Sequence s1, s2, s3;
  EXPECT_CALL(*key_, Equals(fake_key_))
      .InSequence(s1)
      .WillRepeatedly(Return(true));
  ExpectKeyPopulated(s2, true);
  ExpectVerifyAndSetPolicy(s3);

  EXPECT_EQ(TRUE, service_->Store(policy_blob_, NULL, kAllKeyFlags));

  Mock::VerifyAndClearExpectations(key_);
  Mock::VerifyAndClearExpectations(store_);

  ExpectPersistPolicy(s2);
  loop_.RunAllPending();
}

TEST_F(PolicyServiceTest, StoreNewKeyNotAllowed) {
  InitPolicy(fake_data_, fake_sig_, fake_key_, "");

  Sequence s1, s2;
  ExpectKeyEqualsFalse(s1);
  ExpectKeyPopulated(s2, false);

  ExpectStoreFail(0, CHROMEOS_LOGIN_ERROR_ILLEGAL_PUBKEY);
}

TEST_F(PolicyServiceTest, StoreRotation) {
  InitPolicy(fake_data_, fake_sig_, fake_key_, fake_key_sig_);

  Sequence s1, s2;
  ExpectKeyEqualsFalse(s1);
  ExpectKeyPopulated(s2, true);
  EXPECT_CALL(*key_, Rotate(VectorEq(fake_key_), VectorEq(fake_key_sig_)))
      .InSequence(s1, s2)
      .WillOnce(Return(true));
  ExpectKeyPopulated(s1, true);
  ExpectVerifyAndSetPolicy(s2);

  EXPECT_EQ(TRUE, service_->Store(policy_blob_, NULL, kAllKeyFlags));

  Mock::VerifyAndClearExpectations(key_);
  Mock::VerifyAndClearExpectations(store_);

  ExpectPersistKey(s1);
  ExpectPersistPolicy(s2);
  loop_.RunAllPending();
}

TEST_F(PolicyServiceTest, StoreRotationClobber) {
  InitPolicy(fake_data_, fake_sig_, fake_key_, fake_key_sig_);

  Sequence s1, s2;
  ExpectKeyEqualsFalse(s1);
  ExpectKeyPopulated(s2, false);
  EXPECT_CALL(*key_, ClobberCompromisedKey(VectorEq(fake_key_)))
      .InSequence(s1, s2)
      .WillOnce(Return(true));
  ExpectKeyPopulated(s1, true);
  ExpectVerifyAndSetPolicy(s2);

  EXPECT_EQ(TRUE, service_->Store(policy_blob_, NULL,
                                  PolicyService::KEY_CLOBBER));

  Mock::VerifyAndClearExpectations(key_);
  Mock::VerifyAndClearExpectations(store_);

  ExpectPersistKey(s1);
  ExpectPersistPolicy(s2);
  loop_.RunAllPending();
}

TEST_F(PolicyServiceTest, StoreRotationNoSignature) {
  InitPolicy(fake_data_, fake_sig_, fake_key_, "");

  Sequence s1, s2;
  ExpectKeyEqualsFalse(s1);
  ExpectKeyPopulated(s2, true);

  ExpectStoreFail(PolicyService::KEY_ROTATE,
                  CHROMEOS_LOGIN_ERROR_ILLEGAL_PUBKEY);
}

TEST_F(PolicyServiceTest, StoreRotationBadSignature) {
  InitPolicy(fake_data_, fake_sig_, fake_key_, fake_key_sig_);

  Sequence s1, s2;
  ExpectKeyEqualsFalse(s1);
  ExpectKeyPopulated(s2, true);
  EXPECT_CALL(*key_, Rotate(VectorEq(fake_key_), VectorEq(fake_key_sig_)))
      .InSequence(s1, s2)
      .WillOnce(Return(false));

  ExpectStoreFail(PolicyService::KEY_ROTATE,
                  CHROMEOS_LOGIN_ERROR_ILLEGAL_PUBKEY);
}

TEST_F(PolicyServiceTest, StoreRotationNotAllowed) {
  InitPolicy(fake_data_, fake_sig_, fake_key_, fake_key_sig_);

  Sequence s1, s2;
  ExpectKeyEqualsFalse(s1);
  ExpectKeyPopulated(s2, true);

  ExpectStoreFail(0, CHROMEOS_LOGIN_ERROR_ILLEGAL_PUBKEY);
}

TEST_F(PolicyServiceTest, Retrieve) {
  InitPolicy(fake_data_, fake_sig_, fake_key_, fake_key_sig_);

  EXPECT_CALL(*store_, Get())
      .WillOnce(ReturnRef(policy_proto_));

  GArray* out_policy = NULL;
  GError* error = NULL;
  EXPECT_EQ(TRUE, service_->Retrieve(&out_policy, &error));
  ASSERT_TRUE(out_policy);
  ASSERT_FALSE(error);
  ASSERT_EQ(policy_str_.size(), out_policy->len);
  EXPECT_TRUE(std::equal(policy_str_.begin(), policy_str_.end(),
                         out_policy->data));
  g_array_free(out_policy, TRUE);
}

class PolicyServiceThreadTest : public testing::Test {
 public:
  PolicyServiceThreadTest()
      : io_thread_("policy service test IO thread") {
    io_thread_.Start();
  }

  virtual void SetUp() {
    key_ = new StrictMock<MockOwnerKey>;
    store_ = new StrictMock<MockPolicyStore>;
    system_ = new MockSystemUtils;
    service_ =
        new PolicyService(store_, key_, system_,
                          base::MessageLoopProxy::CreateForCurrentThread(),
                          io_thread_.message_loop_proxy());
  }

  MessageLoop main_loop_;
  base::Thread io_thread_;

  // Use StrictMock to make sure that no unexpected policy or key mutations can
  // occur without the test failing.
  StrictMock<MockOwnerKey>* key_;
  StrictMock<MockPolicyStore>* store_;
  MockSystemUtils* system_;

  scoped_refptr<PolicyService> service_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PolicyServiceThreadTest);
};

TEST_F(PolicyServiceThreadTest, PersistPolicySyncSuccess) {
  EXPECT_CALL(*store_, Persist())
      .WillOnce(Return(true));
  EXPECT_TRUE(service_->PersistPolicySync());
  main_loop_.RunAllPending();
}

TEST_F(PolicyServiceThreadTest, PersistPolicySyncFailure) {
  EXPECT_CALL(*store_, Persist())
      .WillOnce(Return(false));
  EXPECT_FALSE(service_->PersistPolicySync());
  main_loop_.RunAllPending();
}

}  // namespace login_manager
