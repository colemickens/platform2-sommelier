// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "slot_manager_impl.h"

#include <string>

#include <base/memory/scoped_ptr.h>
#include <base/stringprintf.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <openssl/sha.h>

#include "chaps_factory_mock.h"
#include "chaps_utility.h"
#include "isolate.h"
#include "object_importer_mock.h"
#include "object_pool_mock.h"
#include "object_store_mock.h"
#include "session_mock.h"
#include "tpm_utility_mock.h"

using chromeos::SecureBlob;
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

SecureBlob MakeBlob(const char* auth_data_str) {
  return Sha1(SecureBlob(auth_data_str, strlen(auth_data_str)));
}

// Creates and sets default expectations on a ObjectPoolMock instance. Returns
// a pointer to the new object.
ObjectPool* CreateObjectPoolMock() {
  ObjectPoolMock* object_pool = new ObjectPoolMock();
  EXPECT_CALL(*object_pool, GetInternalBlob(kEncryptedAuthKey, _))
      .WillRepeatedly(DoAll(SetArgumentPointee<1>(string("auth_key_blob")),
                            Return(true)));
  EXPECT_CALL(*object_pool, GetInternalBlob(kEncryptedMasterKey, _))
      .WillRepeatedly(
          DoAll(SetArgumentPointee<1>(string("encrypted_master_key")),
                Return(true)));
  EXPECT_CALL(*object_pool, GetInternalBlob(kImportedTracker, _))
      .WillRepeatedly(DoAll(SetArgumentPointee<1>(string()), Return(false)));
  EXPECT_CALL(*object_pool, GetInternalBlob(kAuthDataHash, _))
      .WillRepeatedly(DoAll(SetArgumentPointee<1>(string("\x01\xCE")),
                Return(true)));
  EXPECT_CALL(*object_pool,
              SetInternalBlob(kEncryptedAuthKey, string("auth_key_blob")))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*object_pool,
              SetInternalBlob(kEncryptedAuthKey, string("new_auth_key_blob")))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*object_pool, SetInternalBlob(kEncryptedMasterKey,
                                            string("encrypted_master_key")))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*object_pool, SetInternalBlob(kImportedTracker, string()))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*object_pool, SetInternalBlob(kAuthDataHash, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*object_pool, SetEncryptionKey(_))
      .WillRepeatedly(Return(true));
  return object_pool;
}

// Sets default expectations on a TPMUtilityMock.
void ConfigureTPMUtility(TPMUtilityMock* tpm) {
  EXPECT_CALL(*tpm, Init()).WillRepeatedly(Return(true));
  EXPECT_CALL(*tpm, UnloadKeysForSlot(_)).Times(AnyNumber());
  EXPECT_CALL(*tpm, Authenticate(_,
                                 Sha1(MakeBlob(kAuthData)),
                                 string("auth_key_blob"),
                                 string("encrypted_master_key"),
                                 _))
      .WillRepeatedly(DoAll(SetArgumentPointee<4>(MakeBlob("master_key")),
                            Return(true)));
  EXPECT_CALL(*tpm, ChangeAuthData(_,
                                   Sha1(MakeBlob(kAuthData)),
                                   Sha1(MakeBlob(kNewAuthData)),
                                   string("auth_key_blob"),
                                   _))
      .WillRepeatedly(
          DoAll(SetArgumentPointee<4>(string("new_auth_key_blob")),
                Return(true)));
  EXPECT_CALL(*tpm, GenerateRandom(_, _))
      .WillRepeatedly(DoAll(SetArgumentPointee<1>(string("master_key")),
                            Return(true)));
  string exponent(kDefaultPubExp, kDefaultPubExpSize);
  EXPECT_CALL(*tpm, GenerateKey(1, 2048, exponent, MakeBlob(kAuthData), _, _))
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
    ic_ = IsolateCredentialManager::GetDefaultIsolateCredential();
  }
  void SetUp() {
    slot_manager_.reset(new SlotManagerImpl(&factory_, &tpm_));
    ASSERT_TRUE(slot_manager_->Init());
  }
#if GTEST_IS_THREADSAFE
  void InsertToken() {
    slot_manager_->LoadToken(ic_, FilePath("/var/lib/chaps"),
                             MakeBlob(kAuthData));
  }
#endif

 protected:
  ChapsFactoryMock factory_;
  TPMUtilityMock tpm_;
  scoped_ptr<SlotManagerImpl> slot_manager_;
  SecureBlob ic_;
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
  EXPECT_DEATH_IF_SUPPORTED(slot_manager_->IsTokenPresent(ic_, 3),
                            "Check failed");
  EXPECT_DEATH_IF_SUPPORTED(slot_manager_->GetSlotInfo(ic_, 0, NULL),
                            "Check failed");
  CK_SLOT_INFO slot_info;
  EXPECT_DEATH_IF_SUPPORTED(slot_manager_->GetSlotInfo(ic_, 3, &slot_info),
                            "Check failed");
  EXPECT_DEATH_IF_SUPPORTED(slot_manager_->GetTokenInfo(ic_, 0, NULL),
                            "Check failed");
  CK_TOKEN_INFO token_info;
  EXPECT_DEATH_IF_SUPPORTED(slot_manager_->GetTokenInfo(ic_, 3, &token_info),
                            "Check failed");
  EXPECT_DEATH_IF_SUPPORTED(slot_manager_->GetMechanismInfo(ic_, 3),
                            "Check failed");
  EXPECT_DEATH_IF_SUPPORTED(slot_manager_->OpenSession(ic_, 3, false),
                            "Check failed");
  EXPECT_DEATH_IF_SUPPORTED(slot_manager_->CloseAllSessions(ic_, 3),
                            "Check failed");
  EXPECT_DEATH_IF_SUPPORTED(slot_manager_->GetSession(ic_, 0, NULL),
                            "Check failed");
}

TEST_F(TestSlotManager_DeathTest, OutOfMemorySession) {
  Session* null_session = NULL;
  EXPECT_CALL(factory_, CreateSession(_, _, _, _, _))
      .WillRepeatedly(Return(null_session));
  EXPECT_DEATH_IF_SUPPORTED(slot_manager_->OpenSession(ic_, 0, false),
                            "Check failed");
}

TEST_F(TestSlotManager_DeathTest, NoToken) {
  CK_TOKEN_INFO token_info;
  EXPECT_DEATH_IF_SUPPORTED(slot_manager_->GetTokenInfo(ic_, 1, &token_info),
                            "Check failed");
  EXPECT_DEATH_IF_SUPPORTED(slot_manager_->GetMechanismInfo(ic_, 1),
                            "Check failed");
  EXPECT_DEATH_IF_SUPPORTED(slot_manager_->OpenSession(ic_, 1, false),
                            "Check failed");
}

TEST_F(TestSlotManager, DefaultSlotSetup) {
  EXPECT_EQ(1, slot_manager_->GetSlotCount());
  EXPECT_FALSE(slot_manager_->IsTokenAccessible(ic_, 0));
}

#if GTEST_IS_THREADSAFE

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
  EXPECT_DEATH_IF_SUPPORTED(
      sm.LoadToken(IsolateCredentialManager::GetDefaultIsolateCredential(),
                   FilePath("/var/lib/chaps"),
                   MakeBlob(kAuthData)),
      "Check failed");
}

TEST_F(TestSlotManager, QueryInfo) {
  InsertToken();
  CK_SLOT_INFO slot_info;
  memset(&slot_info, 0xEE, sizeof(slot_info));
  slot_manager_->GetSlotInfo(ic_, 0, &slot_info);
  // Check if all bytes have been set by the call.
  EXPECT_EQ(NULL, memchr(&slot_info, 0xEE, sizeof(slot_info)));
  // TODO(dkrahn): Enable once slot 1 exists (crosbug.com/27759).
  // memset(&slot_info, 0xEE, sizeof(slot_info));
  // slot_manager_->GetSlotInfo(1, &slot_info);
  // EXPECT_EQ(NULL, memchr(&slot_info, 0xEE, sizeof(slot_info)));
  CK_TOKEN_INFO token_info;
  memset(&token_info, 0xEE, sizeof(token_info));
  slot_manager_->GetTokenInfo(ic_, 0, &token_info);
  EXPECT_EQ(NULL, memchr(&token_info, 0xEE, sizeof(token_info)));
  const MechanismMap* mechanisms = slot_manager_->GetMechanismInfo(ic_, 0);
  ASSERT_TRUE(mechanisms != NULL);
  // Sanity check - we don't want to be strict on the mechanism list.
  EXPECT_TRUE(mechanisms->end() != mechanisms->find(CKM_RSA_PKCS));
  EXPECT_TRUE(mechanisms->end() != mechanisms->find(CKM_AES_CBC));
}

TEST_F(TestSlotManager, TestSessions) {
  InsertToken();
  int id1 = slot_manager_->OpenSession(ic_, 0, false);
  int id2 = slot_manager_->OpenSession(ic_, 0, true);
  EXPECT_NE(id1, id2);
  Session* s1 = NULL;
  EXPECT_TRUE(slot_manager_->GetSession(ic_, id1, &s1));
  EXPECT_TRUE(s1 != NULL);
  Session* s2 = NULL;
  EXPECT_TRUE(slot_manager_->GetSession(ic_, id2, &s2));
  EXPECT_TRUE(s2 != NULL);
  EXPECT_NE(s1, s2);
  EXPECT_TRUE(slot_manager_->CloseSession(ic_, id1));
  EXPECT_FALSE(slot_manager_->CloseSession(ic_, id1));
  slot_manager_->CloseAllSessions(ic_, 0);
  EXPECT_FALSE(slot_manager_->CloseSession(ic_, id2));
}

TEST_F(TestSlotManager, TestLoadTokenEvents) {
  InsertToken();
  // TODO(dkrahn): Enable once slot 1 exists (crosbug.com/27759).
  // EXPECT_FALSE(slot_manager_->IsTokenPresent(1));
  slot_manager_->LoadToken(ic_, FilePath("some_path"), MakeBlob(kAuthData));
  EXPECT_TRUE(slot_manager_->IsTokenPresent(ic_, 1));
  // Loat token with an existing path - should be ignored.
  slot_manager_->LoadToken(ic_, FilePath("some_path"), MakeBlob(kAuthData));
  EXPECT_EQ(2, slot_manager_->GetSlotCount());
  slot_manager_->LoadToken(ic_, FilePath("another_path"), MakeBlob(kAuthData));
  EXPECT_TRUE(slot_manager_->IsTokenPresent(ic_, 2));
  slot_manager_->ChangeTokenAuthData(FilePath("some_path"),
                                       MakeBlob(kAuthData),
                                       MakeBlob(kNewAuthData));
  slot_manager_->ChangeTokenAuthData(FilePath("yet_another_path"),
                                       MakeBlob(kAuthData),
                                       MakeBlob(kNewAuthData));
  EXPECT_LT(slot_manager_->GetSlotCount(), 5);
  // Logout with an unknown path.
  slot_manager_->UnloadToken(ic_, FilePath("still_yet_another_path"));
  slot_manager_->UnloadToken(ic_, FilePath("some_path"));
  EXPECT_FALSE(slot_manager_->IsTokenAccessible(ic_, 1));
  EXPECT_TRUE(slot_manager_->IsTokenPresent(ic_, 2));
  slot_manager_->LoadToken(ic_, FilePath("one_more_path"), MakeBlob(kAuthData));
  EXPECT_TRUE(slot_manager_->IsTokenPresent(ic_, 1));
  slot_manager_->UnloadToken(ic_, FilePath("another_path"));
}

#if 0  // TODO(rmcilroy): Reenable this test when thread creation bug is fixed
TEST_F(TestSlotManager, ManyLoadToken) {
  InsertToken();
  for (int i = 0; i < 1000; ++i) {
    string path = base::StringPrintf("test%d", i);
    slot_manager_->LoadToken(ic_, FilePath(path), MakeBlob(kAuthData));
    slot_manager_->ChangeTokenAuthData(FilePath(path), MakeBlob(kAuthData),
                                         MakeBlob(kNewAuthData));
    slot_manager_->ChangeTokenAuthData(FilePath(path + "_"),
                                         MakeBlob(kAuthData),
                                         MakeBlob(kNewAuthData));
  }
  for (int i = 0; i < 1000; ++i) {
    string path = base::StringPrintf("test%d", i);
    slot_manager_->UnloadToken(ic_, FilePath(path));
  }
}
#endif

TEST_F(TestSlotManager, TestTPMFailure) {
  EXPECT_CALL(tpm_, Init()).WillRepeatedly(Return(false));
  InsertToken();
  EXPECT_FALSE(slot_manager_->IsTokenAccessible(ic_, 0));
}

TEST_F(TestSlotManager, TestDefaultIsolate) {
  // Check default isolate is there by default.
  SecureBlob defaultIsolate =
      IsolateCredentialManager::GetDefaultIsolateCredential();
  EXPECT_TRUE(slot_manager_->OpenIsolate(&defaultIsolate));
  EXPECT_EQ(IsolateCredentialManager::GetDefaultIsolateCredential(),
            defaultIsolate);
}

TEST_F(TestSlotManager, TestOpenIsolate) {
  EXPECT_CALL(tpm_, GenerateRandom(_, _))
      .WillOnce(DoAll(SetArgumentPointee<1>(string("567890")), Return(true)));

  // Check that trying to open an invalid isolate creates new isolate.
  SecureBlob isolate("invalid");
  EXPECT_FALSE(slot_manager_->OpenIsolate(&isolate));
  EXPECT_EQ(SecureBlob(string("567890")), isolate);

  // Check opening an existing isolate.
  EXPECT_TRUE(slot_manager_->OpenIsolate(&isolate));
  EXPECT_EQ(SecureBlob(string("567890")), isolate);
}

TEST_F(TestSlotManager, TestCloseIsolate) {
  EXPECT_CALL(tpm_, GenerateRandom(_, _))
      .WillOnce(DoAll(SetArgumentPointee<1>(string("abcdef")), Return(true)))
      .WillOnce(DoAll(SetArgumentPointee<1>(string("ghijkl")), Return(true)));

  SecureBlob isolate;
  EXPECT_FALSE(slot_manager_->OpenIsolate(&isolate));
  EXPECT_EQ(SecureBlob(string("abcdef")), isolate);
  EXPECT_TRUE(slot_manager_->OpenIsolate(&isolate));
  EXPECT_EQ(SecureBlob(string("abcdef")), isolate);
  slot_manager_->CloseIsolate(isolate);
  slot_manager_->CloseIsolate(isolate);
  // Final logout, isolate should now be destroyed.
  EXPECT_FALSE(slot_manager_->OpenIsolate(&isolate));
  EXPECT_EQ(SecureBlob(string("ghijkl")), isolate);
}

TEST_F(TestSlotManager, TestCloseIsolateUnloadToken) {
  SecureBlob isolate;
  EXPECT_FALSE(slot_manager_->OpenIsolate(&isolate));
  EXPECT_FALSE(slot_manager_->IsTokenAccessible(isolate, 0));
  slot_manager_->LoadToken(isolate, FilePath("some_path"), MakeBlob(kAuthData));
  EXPECT_TRUE(slot_manager_->IsTokenPresent(isolate, 0));
  // Token should be unloaded by CloseIsolate call.
  slot_manager_->CloseIsolate(isolate);
  EXPECT_FALSE(slot_manager_->IsTokenAccessible(isolate, 0));
}

TEST_F(TestSlotManager_DeathTest, TestIsolateTokens) {
  CK_SLOT_INFO slot_info;
  CK_TOKEN_INFO token_info;
  Session* session;
  SecureBlob new_isolate_0, new_isolate_1;
  SecureBlob defaultIsolate =
      IsolateCredentialManager::GetDefaultIsolateCredential();

  // Ensure different credentials are created for each isolate.
  EXPECT_CALL(tpm_, GenerateRandom(_, _))
      .WillOnce(DoAll(SetArgumentPointee<1>(string("123456")), Return(true)))
      .WillOnce(DoAll(SetArgumentPointee<1>(string("567890")), Return(true)));

  EXPECT_FALSE(slot_manager_->OpenIsolate(&new_isolate_0));
  slot_manager_->LoadToken(new_isolate_0, FilePath("new_isolate"),
                           MakeBlob(kAuthData));
  EXPECT_EQ(1, slot_manager_->GetSlotCount());

  EXPECT_FALSE(slot_manager_->OpenIsolate(&new_isolate_1));
  slot_manager_->LoadToken(new_isolate_1, FilePath("another_new_isolate"),
                           MakeBlob(kAuthData));
  EXPECT_EQ(2, slot_manager_->GetSlotCount());

  // Ensure tokens are only accessable with the valid isolate cred.
  EXPECT_TRUE(slot_manager_->IsTokenAccessible(new_isolate_0, 0));
  EXPECT_TRUE(slot_manager_->IsTokenAccessible(new_isolate_1, 1));
  EXPECT_FALSE(slot_manager_->IsTokenAccessible(new_isolate_1, 0));
  EXPECT_FALSE(slot_manager_->IsTokenAccessible(new_isolate_0, 1));
  EXPECT_FALSE(slot_manager_->IsTokenAccessible(defaultIsolate, 0));
  EXPECT_FALSE(slot_manager_->IsTokenAccessible(defaultIsolate, 1));

  // Check all public methods perform isolate checks.
  EXPECT_DEATH_IF_SUPPORTED(
      slot_manager_->IsTokenPresent(new_isolate_0, 1), "Check failed");
  EXPECT_DEATH_IF_SUPPORTED(
      slot_manager_->GetSlotInfo(new_isolate_0, 1, &slot_info),
      "Check failed");
  EXPECT_DEATH_IF_SUPPORTED(
      slot_manager_->GetTokenInfo(new_isolate_0, 1, &token_info),
      "Check failed");
  EXPECT_DEATH_IF_SUPPORTED(
      slot_manager_->GetMechanismInfo(new_isolate_0, 1), "Check failed");
  EXPECT_DEATH_IF_SUPPORTED(
      slot_manager_->OpenSession(new_isolate_0, 1, false), "Check failed");
  int slot_1_session = slot_manager_->OpenSession(new_isolate_1, 1, false);
  EXPECT_TRUE(
      slot_manager_->GetSession(new_isolate_1, slot_1_session, &session));
  EXPECT_FALSE(
      slot_manager_->GetSession(new_isolate_0, slot_1_session, &session));
  EXPECT_FALSE(slot_manager_->CloseSession(new_isolate_0, slot_1_session));
  EXPECT_DEATH_IF_SUPPORTED(
      slot_manager_->CloseAllSessions(new_isolate_0, 1), "Check failed");
}
#endif

}  // namespace chaps

int main(int argc, char** argv) {
  ::testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}
