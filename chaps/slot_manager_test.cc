// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "slot_manager_impl.h"

#include <string>

#include <base/scoped_ptr.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "chaps_factory_mock.h"
#include "object_pool_mock.h"
#include "session_mock.h"
#include "tpm_utility_mock.h"

using std::string;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::SetArgumentPointee;

namespace chaps {

static const string kAuthData("000000");
static const string kNewAuthData("111111");

static ObjectPool* CreateObjectPoolMock() {
  ObjectPoolMock* op = new ObjectPoolMock();
  EXPECT_CALL(*op, GetInternalBlob(0, _))
      .WillRepeatedly(DoAll(SetArgumentPointee<1>(string("auth_key_blob")),
                            Return(true)));
  EXPECT_CALL(*op, GetInternalBlob(1, _))
      .WillRepeatedly(
          DoAll(SetArgumentPointee<1>(string("encrypted_master_key")),
                Return(true)));
  EXPECT_CALL(*op, SetInternalBlob(0, string("auth_key_blob")))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*op, SetInternalBlob(0, string("new_auth_key_blob")))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*op, SetInternalBlob(1, string("encrypted_master_key")))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*op, SetKey(string("master_key"))).Times(AnyNumber());
  return op;
}

static void ConfigureTPMUtility(TPMUtilityMock* tpm) {
  EXPECT_CALL(*tpm, Authenticate(kAuthData,
                                 string("auth_key_blob"),
                                 string("encrypted_master_key"),
                                 _))
      .WillRepeatedly(DoAll(SetArgumentPointee<3>(string("master_key")),
                            Return(true)));
  EXPECT_CALL(*tpm, ChangeAuthData(kAuthData,
                                   kNewAuthData,
                                   string("auth_key_blob"),
                                   _))
      .WillRepeatedly(
          DoAll(SetArgumentPointee<3>(string("new_auth_key_blob")),
                Return(true)));
  EXPECT_CALL(*tpm, GenerateRandom(_, _))
      .WillRepeatedly(DoAll(SetArgumentPointee<1>(string("master_key")),
                            Return(true)));
  EXPECT_CALL(*tpm, GenerateKey(kAuthData, _))
      .WillRepeatedly(DoAll(SetArgumentPointee<1>(string("auth_key_blob")),
                            Return(true)));
  EXPECT_CALL(*tpm, Bind(string("auth_key_blob"),
                         kAuthData,
                         string("master_key"),
                         _))
      .WillRepeatedly(
          DoAll(SetArgumentPointee<3>(string("encrypted_master_key")),
                Return(true)));
}

static bool IsAnyByteEqualTo(void* buffer, int length, uint8_t value) {
  uint8_t* bytes = reinterpret_cast<uint8_t*>(buffer);
  for (int i = 0; i < length; ++i) {
    if (bytes[i] == value)
      return true;
  }
  return false;
}

static Session* CreateNewSession() {
  return new SessionMock();
}

// Test fixture for an initialized SlotManagerImpl instance.
class TestSlotManager: public ::testing::Test {
 public:
  TestSlotManager() {
    EXPECT_CALL(factory_, CreateSession(_, _, _, _))
        .WillRepeatedly(InvokeWithoutArgs(CreateNewSession));
    EXPECT_CALL(factory_, CreatePersistentObjectPool(_))
        .WillRepeatedly(InvokeWithoutArgs(CreateObjectPoolMock));
    ConfigureTPMUtility(&tpm_);
  }
  void SetUp() {
    slot_manager_.reset(new SlotManagerImpl(&factory_, &tpm_));
    ASSERT_TRUE(slot_manager_->Init());
  }

 protected:
  ChapsFactoryMock factory_;
  TPMUtilityMock tpm_;
  scoped_ptr<SlotManagerImpl> slot_manager_;
};

typedef TestSlotManager TestSlotManager_DeathTest;

TEST(DeathTest, InvalidInit) {
  ChapsFactoryMock factory;
  SlotManagerImpl sm(&factory, NULL);
  EXPECT_DEATH_IF_SUPPORTED(sm.Init(), "Check failed");
  TPMUtilityMock tpm;
  SlotManagerImpl sm2(NULL, &tpm);
  EXPECT_DEATH_IF_SUPPORTED(sm.Init(), "Check failed");
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
  EXPECT_CALL(factory, CreatePersistentObjectPool(_))
      .WillRepeatedly(Return(null_pool));
  SlotManagerImpl sm(&factory, &tpm);
  EXPECT_DEATH_IF_SUPPORTED(sm.Init(), "Check failed");
}

TEST_F(TestSlotManager_DeathTest, OutOfMemorySession) {
  Session* null_session = NULL;
  EXPECT_CALL(factory_, CreateSession(_, _, _, _))
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
  EXPECT_EQ(2, slot_manager_->GetSlotCount());
  EXPECT_TRUE(slot_manager_->IsTokenPresent(0));
  EXPECT_FALSE(slot_manager_->IsTokenPresent(1));
}

TEST_F(TestSlotManager, QueryInfo) {
  CK_SLOT_INFO slot_info;
  memset(&slot_info, 0xEE, sizeof(slot_info));
  slot_manager_->GetSlotInfo(0, &slot_info);
  // Check if all bytes have been set by the call.
  EXPECT_FALSE(IsAnyByteEqualTo(&slot_info, sizeof(slot_info), 0xEE));
  memset(&slot_info, 0xEE, sizeof(slot_info));
  slot_manager_->GetSlotInfo(1, &slot_info);
  EXPECT_FALSE(IsAnyByteEqualTo(&slot_info, sizeof(slot_info), 0xEE));
  CK_TOKEN_INFO token_info;
  memset(&token_info, 0xEE, sizeof(token_info));
  slot_manager_->GetTokenInfo(0, &token_info);
  EXPECT_FALSE(IsAnyByteEqualTo(&token_info, sizeof(token_info), 0xEE));
  const MechanismMap* mechanisms = slot_manager_->GetMechanismInfo(0);
  ASSERT_TRUE(mechanisms != NULL);
  // Sanity check - we don't want to be strict on the mechanism list.
  EXPECT_TRUE(mechanisms->end() != mechanisms->find(CKM_RSA_PKCS));
  EXPECT_TRUE(mechanisms->end() != mechanisms->find(CKM_AES_CBC));
}

TEST_F(TestSlotManager, TestSessions) {
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
  EXPECT_FALSE(slot_manager_->IsTokenPresent(1));
  slot_manager_->OnLogin("some_path", kAuthData);
  EXPECT_TRUE(slot_manager_->IsTokenPresent(1));
  // Login with an existing path - should be ignored.
  slot_manager_->OnLogin("some_path", kAuthData);
  EXPECT_EQ(2, slot_manager_->GetSlotCount());
  slot_manager_->OnLogin("another_path", kAuthData);
  EXPECT_TRUE(slot_manager_->IsTokenPresent(2));
  slot_manager_->OnChangeAuthData("some_path", kAuthData, kNewAuthData);
  slot_manager_->OnChangeAuthData("yet_another_path", kAuthData, kNewAuthData);
  EXPECT_EQ(3, slot_manager_->GetSlotCount());
  // Logout with an unknown path.
  slot_manager_->OnLogout("still_yet_another_path");
  slot_manager_->OnLogout("some_path");
  EXPECT_FALSE(slot_manager_->IsTokenPresent(1));
  EXPECT_TRUE(slot_manager_->IsTokenPresent(2));
  slot_manager_->OnLogin("one_more_path", kAuthData);
  EXPECT_TRUE(slot_manager_->IsTokenPresent(1));
  slot_manager_->OnLogout("another_path");
}

}  // namespace chaps

int main(int argc, char** argv) {
  ::testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}
