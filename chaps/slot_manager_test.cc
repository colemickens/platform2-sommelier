// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "slot_manager_impl.h"

#include <string>

#include <base/memory/scoped_ptr.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <openssl/sha.h>

#include "chaps_factory_mock.h"
#include "chaps_utility.h"
#include "object_importer_mock.h"
#include "object_pool_mock.h"
#include "object_store_mock.h"
#include "session_mock.h"
#include "tpm_utility_mock.h"

using std::string;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::SetArgumentPointee;

namespace chaps {

namespace {

const char kAuthData[] = "000000";
const char kNewAuthData[] = "111111";
const char kDefaultPubExp[] = {1, 0, 1};
const int kDefaultPubExpSize = 3;

// Creates and sets default expectations on a ObjectPoolMock instance. Returns
// a pointer to the new object.
ObjectPool* CreateObjectPoolMock() {
  ObjectPoolMock* object_pool = new ObjectPoolMock();
  EXPECT_CALL(*object_pool, GetInternalBlob(0, _))
      .WillRepeatedly(DoAll(SetArgumentPointee<1>(string("auth_key_blob")),
                            Return(true)));
  EXPECT_CALL(*object_pool, GetInternalBlob(1, _))
      .WillRepeatedly(
          DoAll(SetArgumentPointee<1>(string("encrypted_master_key")),
                Return(true)));
  EXPECT_CALL(*object_pool, GetInternalBlob(2, _))
      .WillRepeatedly(DoAll(SetArgumentPointee<1>(string()), Return(false)));
  EXPECT_CALL(*object_pool, SetInternalBlob(0, string("auth_key_blob")))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*object_pool, SetInternalBlob(0, string("new_auth_key_blob")))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*object_pool, SetInternalBlob(1, string("encrypted_master_key")))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*object_pool, SetInternalBlob(2, string()))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*object_pool, SetEncryptionKey(string("master_key")))
      .WillRepeatedly(Return(true));
  return object_pool;
}

// Sets default expectations on a TPMUtilityMock.
void ConfigureTPMUtility(TPMUtilityMock* tpm) {
  EXPECT_CALL(*tpm, UnloadKeysForSlot(_)).Times(AnyNumber());
  EXPECT_CALL(*tpm, Authenticate(_,
                                 Sha1(kAuthData),
                                 string("auth_key_blob"),
                                 string("encrypted_master_key"),
                                 _))
      .WillRepeatedly(DoAll(SetArgumentPointee<4>(string("master_key")),
                            Return(true)));
  EXPECT_CALL(*tpm, ChangeAuthData(_,
                                   Sha1(kAuthData),
                                   Sha1(kNewAuthData),
                                   string("auth_key_blob"),
                                   _))
      .WillRepeatedly(
          DoAll(SetArgumentPointee<4>(string("new_auth_key_blob")),
                Return(true)));
  EXPECT_CALL(*tpm, GenerateRandom(_, _))
      .WillRepeatedly(DoAll(SetArgumentPointee<1>(string("master_key")),
                            Return(true)));
  string exponent(kDefaultPubExp, kDefaultPubExpSize);
  EXPECT_CALL(*tpm, GenerateKey(1, 2048, exponent, kAuthData, _, _))
      .WillRepeatedly(DoAll(SetArgumentPointee<4>(string("auth_key_blob")),
                            SetArgumentPointee<5>(1),
                            Return(true)));
  EXPECT_CALL(*tpm, Bind(1,
                         string("master_key"),
                         _))
      .WillRepeatedly(
          DoAll(SetArgumentPointee<2>(string("encrypted_master_key")),
                Return(true)));
}

// Creates and returns a mock Session instance.
Session* CreateNewSession() {
  return new SessionMock();
}

}  // namespace

// A test fixture for an initialized SlotManagerImpl instance.
class TestSlotManager: public ::testing::Test {
 public:
  TestSlotManager() {
    EXPECT_CALL(factory_, CreateSession(_, _, _, _, _))
        .WillRepeatedly(InvokeWithoutArgs(CreateNewSession));
    EXPECT_CALL(factory_, CreateObjectPool(_, _, _))
        .WillRepeatedly(InvokeWithoutArgs(CreateObjectPoolMock));
    ObjectStore* null_store = NULL;
    EXPECT_CALL(factory_, CreateObjectStore(_))
        .WillRepeatedly(Return(null_store));
    ObjectImporter* null_importer = NULL;
    EXPECT_CALL(factory_, CreateObjectImporter(_, _, _))
        .WillRepeatedly(Return(null_importer));
    ConfigureTPMUtility(&tpm_);
  }
  void SetUp() {
    slot_manager_.reset(new SlotManagerImpl(&factory_, &tpm_));
    ASSERT_TRUE(slot_manager_->Init());
  }
  void InsertToken() {
    slot_manager_->OnLogin(FilePath("/var/lib/chaps"), kAuthData);
  }

 protected:
  ChapsFactoryMock factory_;
  TPMUtilityMock tpm_;
  scoped_ptr<SlotManagerImpl> slot_manager_;
};

typedef TestSlotManager TestSlotManager_DeathTest;

TEST(DeathTest, InvalidInit) {
  ChapsFactoryMock factory;
  EXPECT_DEATH_IF_SUPPORTED(new SlotManagerImpl(&factory, NULL),
                            "Check failed");
  TPMUtilityMock tpm;
  EXPECT_DEATH_IF_SUPPORTED(new SlotManagerImpl(NULL, &tpm), "Check failed");
}

TEST_F(TestSlotManager_DeathTest, InvalidArgs) {
  EXPECT_DEATH_IF_SUPPORTED(slot_manager_->IsTokenPresent(3), "Check failed");
  EXPECT_DEATH_IF_SUPPORTED(slot_manager_->GetSlotInfo(0, NULL),
                            "Check failed");
  CK_SLOT_INFO slot_info;
  EXPECT_DEATH_IF_SUPPORTED(slot_manager_->GetSlotInfo(3, &slot_info),
                            "Check failed");
  EXPECT_DEATH_IF_SUPPORTED(slot_manager_->GetTokenInfo(0, NULL),
                            "Check failed");
  CK_TOKEN_INFO token_info;
  EXPECT_DEATH_IF_SUPPORTED(slot_manager_->GetTokenInfo(3, &token_info),
                            "Check failed");
  EXPECT_DEATH_IF_SUPPORTED(slot_manager_->GetMechanismInfo(3), "Check failed");
  EXPECT_DEATH_IF_SUPPORTED(slot_manager_->OpenSession(3, false),
                            "Check failed");
  EXPECT_DEATH_IF_SUPPORTED(slot_manager_->CloseAllSessions(3), "Check failed");
  EXPECT_DEATH_IF_SUPPORTED(slot_manager_->GetSession(0, NULL), "Check failed");
}

TEST(DeathTest, OutOfMemoryInit) {
  TPMUtilityMock tpm;
  ConfigureTPMUtility(&tpm);
  ChapsFactoryMock factory;
  ObjectPool* null_pool = NULL;
  EXPECT_CALL(factory, CreateObjectPool(_, _, _))
      .WillRepeatedly(Return(null_pool));
  ObjectStore* null_store = NULL;
  EXPECT_CALL(factory, CreateObjectStore(_))
      .WillRepeatedly(Return(null_store));
  ObjectImporter* null_importer = NULL;
  EXPECT_CALL(factory, CreateObjectImporter(_, _, _))
      .WillRepeatedly(Return(null_importer));
  SlotManagerImpl sm(&factory, &tpm);
  ASSERT_TRUE(sm.Init());
  EXPECT_DEATH_IF_SUPPORTED(sm.OnLogin(FilePath("/var/lib/chaps"), kAuthData),
                            "Check failed");
}

TEST_F(TestSlotManager_DeathTest, OutOfMemorySession) {
  Session* null_session = NULL;
  EXPECT_CALL(factory_, CreateSession(_, _, _, _, _))
      .WillRepeatedly(Return(null_session));
  EXPECT_DEATH_IF_SUPPORTED(slot_manager_->OpenSession(0, false),
                            "Check failed");
}

TEST_F(TestSlotManager_DeathTest, NoToken) {
  CK_TOKEN_INFO token_info;
  EXPECT_DEATH_IF_SUPPORTED(slot_manager_->GetTokenInfo(1, &token_info),
                            "Check failed");
  EXPECT_DEATH_IF_SUPPORTED(slot_manager_->GetMechanismInfo(1), "Check failed");
  EXPECT_DEATH_IF_SUPPORTED(slot_manager_->OpenSession(1, false),
                            "Check failed");
}

TEST_F(TestSlotManager, DefaultSlotSetup) {
  EXPECT_EQ(1, slot_manager_->GetSlotCount());
  EXPECT_FALSE(slot_manager_->IsTokenPresent(0));
}

TEST_F(TestSlotManager, QueryInfo) {
  InsertToken();
  CK_SLOT_INFO slot_info;
  memset(&slot_info, 0xEE, sizeof(slot_info));
  slot_manager_->GetSlotInfo(0, &slot_info);
  // Check if all bytes have been set by the call.
  EXPECT_EQ(NULL, memchr(&slot_info, 0xEE, sizeof(slot_info)));
  // TODO(dkrahn): Enable once slot 1 exists (crosbug.com/27759).
  // memset(&slot_info, 0xEE, sizeof(slot_info));
  // slot_manager_->GetSlotInfo(1, &slot_info);
  // EXPECT_EQ(NULL, memchr(&slot_info, 0xEE, sizeof(slot_info)));
  CK_TOKEN_INFO token_info;
  memset(&token_info, 0xEE, sizeof(token_info));
  slot_manager_->GetTokenInfo(0, &token_info);
  EXPECT_EQ(NULL, memchr(&token_info, 0xEE, sizeof(token_info)));
  const MechanismMap* mechanisms = slot_manager_->GetMechanismInfo(0);
  ASSERT_TRUE(mechanisms != NULL);
  // Sanity check - we don't want to be strict on the mechanism list.
  EXPECT_TRUE(mechanisms->end() != mechanisms->find(CKM_RSA_PKCS));
  EXPECT_TRUE(mechanisms->end() != mechanisms->find(CKM_AES_CBC));
}

TEST_F(TestSlotManager, TestSessions) {
  InsertToken();
  int id1 = slot_manager_->OpenSession(0, false);
  int id2 = slot_manager_->OpenSession(0, true);
  EXPECT_NE(id1, id2);
  Session* s1 = NULL;
  EXPECT_TRUE(slot_manager_->GetSession(id1, &s1));
  EXPECT_TRUE(s1 != NULL);
  Session* s2 = NULL;
  EXPECT_TRUE(slot_manager_->GetSession(id2, &s2));
  EXPECT_TRUE(s2 != NULL);
  EXPECT_NE(s1, s2);
  EXPECT_TRUE(slot_manager_->CloseSession(id1));
  EXPECT_FALSE(slot_manager_->CloseSession(id1));
  slot_manager_->CloseAllSessions(0);
  EXPECT_FALSE(slot_manager_->CloseSession(id2));
}

TEST_F(TestSlotManager, TestLoginEvents) {
  InsertToken();
  // TODO(dkrahn): Enable once slot 1 exists (crosbug.com/27759).
  // EXPECT_FALSE(slot_manager_->IsTokenPresent(1));
  slot_manager_->OnLogin(FilePath("some_path"), kAuthData);
  EXPECT_TRUE(slot_manager_->IsTokenPresent(1));
  // Login with an existing path - should be ignored.
  slot_manager_->OnLogin(FilePath("some_path"), kAuthData);
  EXPECT_EQ(2, slot_manager_->GetSlotCount());
  slot_manager_->OnLogin(FilePath("another_path"), kAuthData);
  EXPECT_TRUE(slot_manager_->IsTokenPresent(2));
  slot_manager_->OnChangeAuthData(FilePath("some_path"),
                                  kAuthData,
                                  kNewAuthData);
  slot_manager_->OnChangeAuthData(FilePath("yet_another_path"),
                                  kAuthData,
                                  kNewAuthData);
  EXPECT_LT(slot_manager_->GetSlotCount(), 5);
  // Logout with an unknown path.
  slot_manager_->OnLogout(FilePath("still_yet_another_path"));
  slot_manager_->OnLogout(FilePath("some_path"));
  EXPECT_FALSE(slot_manager_->IsTokenPresent(1));
  EXPECT_TRUE(slot_manager_->IsTokenPresent(2));
  slot_manager_->OnLogin(FilePath("one_more_path"), kAuthData);
  EXPECT_TRUE(slot_manager_->IsTokenPresent(1));
  slot_manager_->OnLogout(FilePath("another_path"));
}

}  // namespace chaps

int main(int argc, char** argv) {
  ::testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}
