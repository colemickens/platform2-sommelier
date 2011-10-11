// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Chaps client unit tests. These tests exercise the client layer (chaps.cc) and
// use a mock for the proxy interface so no D-Bus code is run.
//

#include "chaps/chaps_proxy_mock.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "chaps/attributes.h"
#include "chaps/chaps_utility.h"
#include "pkcs11/cryptoki.h"

using std::string;
using std::vector;
using ::testing::_;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::Return;
using ::testing::SetArgumentPointee;

namespace chaps {

static bool SerializeAttributes(CK_ATTRIBUTE_PTR attributes,
                                CK_ULONG num_attributes,
                                string* serialized) {
  Attributes tmp(attributes, num_attributes);
  return tmp.Serialize(serialized);
}

static bool ParseAndFillAttributes(const string& serialized,
                                   CK_ATTRIBUTE_PTR attributes,
                                   CK_ULONG num_attributes) {
  Attributes tmp(attributes, num_attributes);
  return tmp.ParseAndFill(serialized);
}

// Initialize / Finalize tests
TEST(TestInitialize, InitializeNULL) {
  ChapsProxyMock proxy(false);
  EXPECT_EQ(CKR_OK, C_Initialize(NULL_PTR));
  EXPECT_EQ(CKR_OK, C_Finalize(NULL_PTR));
}

TEST(TestInitializeDeathTest, InitializeOutOfMem) {
  EnableMockProxy(NULL, false);
  EXPECT_DEATH_IF_SUPPORTED(C_Initialize(NULL_PTR), "Check failed");
  DisableMockProxy();
}

TEST(TestInitialize, InitializeTwice) {
  ChapsProxyMock proxy(false);
  EXPECT_EQ(CKR_OK, C_Initialize(NULL_PTR));
  EXPECT_EQ(CKR_CRYPTOKI_ALREADY_INITIALIZED, C_Initialize(NULL_PTR));
  EXPECT_EQ(CKR_OK, C_Finalize(NULL_PTR));
}

TEST(TestInitialize, InitializeWithArgs) {
  ChapsProxyMock proxy(false);
  CK_C_INITIALIZE_ARGS args;
  memset(&args, 0, sizeof(args));
  EXPECT_EQ(CKR_OK, C_Initialize(&args));
  EXPECT_EQ(CKR_OK, C_Finalize(NULL_PTR));
}

TEST(TestInitialize, InitializeWithBadArgs) {
  ChapsProxyMock proxy(false);
  CK_C_INITIALIZE_ARGS args;
  memset(&args, 0, sizeof(args));
  args.CreateMutex = (CK_CREATEMUTEX)1;
  EXPECT_EQ(CKR_ARGUMENTS_BAD, C_Initialize(&args));
  memset(&args, 0, sizeof(args));
  args.pReserved = (CK_VOID_PTR)1;
  EXPECT_EQ(CKR_ARGUMENTS_BAD, C_Initialize(&args));
}

TEST(TestInitialize, InitializeNoLocking) {
  ChapsProxyMock proxy(false);
  CK_C_INITIALIZE_ARGS args;
  memset(&args, 0xFF, sizeof(args));
  args.flags = 0;
  args.pReserved = 0;
  EXPECT_EQ(CKR_CANT_LOCK, C_Initialize(&args));
}

TEST(TestInitialize, FinalizeWithArgs) {
  EXPECT_EQ(CKR_ARGUMENTS_BAD, C_Finalize((void*)1));
}

TEST(TestInitialize, FinalizeNotInit) {
  EXPECT_EQ(CKR_CRYPTOKI_NOT_INITIALIZED, C_Finalize(NULL_PTR));
}

TEST(TestInitialize, Reinitialize) {
  ChapsProxyMock proxy(false);
  EXPECT_EQ(CKR_OK, C_Initialize(NULL_PTR));
  EXPECT_EQ(CKR_OK, C_Finalize(NULL_PTR));
  EXPECT_EQ(CKR_OK, C_Initialize(NULL_PTR));
}

// Library Information Tests
TEST(TestLibInfo, LibInfoOK) {
  ChapsProxyMock proxy(true);
  CK_INFO info;
  EXPECT_EQ(CKR_OK, C_GetInfo(&info));
}

TEST(TestLibInfo, LibInfoNull) {
  ChapsProxyMock proxy(true);
  EXPECT_EQ(CKR_ARGUMENTS_BAD, C_GetInfo(NULL));
}

TEST(TestLibInfo, LibInfoNotInit) {
  ChapsProxyMock proxy(false);
  CK_INFO info;
  EXPECT_EQ(CKR_CRYPTOKI_NOT_INITIALIZED, C_GetInfo(&info));
}

// Slot List Tests
class TestSlotList : public ::testing::Test {
protected:
  virtual void SetUp() {
    uint32_t slot_array[3] = {1, 2, 3};
    slot_list_all_.assign(&slot_array[0], &slot_array[3]);
    slot_list_present_.assign(&slot_array[1], &slot_array[3]);
  }
  vector<uint32_t> slot_list_all_;
  vector<uint32_t> slot_list_present_;
};

TEST_F(TestSlotList, SlotListOK) {
  ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, GetSlotList(false, _))
      .WillOnce(DoAll(SetArgumentPointee<1>(slot_list_all_),
                      Return(CKR_OK)));
  CK_SLOT_ID slots[3];
  CK_ULONG num_slots = 3;
  EXPECT_EQ(CKR_OK, C_GetSlotList(CK_FALSE, slots, &num_slots));
  EXPECT_EQ(num_slots, slot_list_all_.size());
  EXPECT_EQ(slots[0], slot_list_all_[0]);
  EXPECT_EQ(slots[1], slot_list_all_[1]);
  EXPECT_EQ(slots[2], slot_list_all_[2]);
}

TEST_F(TestSlotList, SlotListNull) {
  ChapsProxyMock proxy(true);
  EXPECT_EQ(CKR_ARGUMENTS_BAD, C_GetSlotList(CK_FALSE, NULL, NULL));
}

TEST_F(TestSlotList, SlotListNotInit) {
  ChapsProxyMock proxy(false);
  CK_SLOT_ID slots[3];
  CK_ULONG num_slots = 3;
  EXPECT_EQ(CKR_CRYPTOKI_NOT_INITIALIZED,
            C_GetSlotList(CK_FALSE, slots, &num_slots));
}

TEST_F(TestSlotList, SlotListNoBuffer) {
  ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, GetSlotList(false, _))
      .WillOnce(DoAll(SetArgumentPointee<1>(slot_list_all_),
                      Return(CKR_OK)));
  CK_ULONG num_slots = 17;
  EXPECT_EQ(CKR_OK, C_GetSlotList(CK_FALSE, NULL, &num_slots));
  EXPECT_EQ(num_slots, slot_list_all_.size());
}

TEST_F(TestSlotList, SlotListSmallBuffer) {
  ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, GetSlotList(false, _))
      .WillOnce(DoAll(SetArgumentPointee<1>(slot_list_all_),
                      Return(CKR_OK)));
  CK_SLOT_ID slots[2];
  CK_ULONG num_slots = 2;
  EXPECT_EQ(CKR_BUFFER_TOO_SMALL, C_GetSlotList(CK_FALSE, slots, &num_slots));
  EXPECT_EQ(num_slots, slot_list_all_.size());
}

TEST_F(TestSlotList, SlotListLargeBuffer) {
  ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, GetSlotList(false, _))
      .WillOnce(DoAll(SetArgumentPointee<1>(slot_list_all_),
                      Return(CKR_OK)));
  CK_SLOT_ID slots[4];
  CK_ULONG num_slots = 4;
  EXPECT_EQ(CKR_OK, C_GetSlotList(CK_FALSE, slots, &num_slots));
  EXPECT_EQ(num_slots, slot_list_all_.size());
  EXPECT_EQ(slots[0], slot_list_all_[0]);
  EXPECT_EQ(slots[1], slot_list_all_[1]);
  EXPECT_EQ(slots[2], slot_list_all_[2]);
}

TEST_F(TestSlotList, SlotListPresentOnly) {
  ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, GetSlotList(true, _))
      .WillOnce(DoAll(SetArgumentPointee<1>(slot_list_present_),
                      Return(CKR_OK)));
  CK_SLOT_ID slots[4];
  CK_ULONG num_slots = 4;
  EXPECT_EQ(CKR_OK, C_GetSlotList(CK_TRUE, slots, &num_slots));
  EXPECT_EQ(num_slots, slot_list_present_.size());
  EXPECT_EQ(slots[0], slot_list_present_[0]);
  EXPECT_EQ(slots[1], slot_list_present_[1]);
}

TEST_F(TestSlotList, SlotListFailure) {
  ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, GetSlotList(false, _))
      .WillOnce(DoAll(SetArgumentPointee<1>(slot_list_present_),
                      Return(CKR_FUNCTION_FAILED)));
  CK_SLOT_ID slots[4];
  CK_ULONG num_slots = 4;
  EXPECT_EQ(CKR_FUNCTION_FAILED, C_GetSlotList(CK_FALSE, slots, &num_slots));
}

// Slot Info Tests
TEST(TestSlotInfo, SlotInfoOK) {
  ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, GetSlotInfo(1, _, _, _, _, _, _, _))
      .WillOnce(DoAll(SetArgumentPointee<3>(1), Return(CKR_OK)));
  CK_SLOT_INFO info;
  memset(&info, 0, sizeof(info));
  EXPECT_EQ(CKR_OK, C_GetSlotInfo(1, &info));
  EXPECT_EQ(string(64, ' '), string((char*)info.slotDescription, 64));
  EXPECT_EQ(string(32, ' '), string((char*)info.manufacturerID, 32));
  EXPECT_EQ(1, info.flags);
}

TEST(TestSlotInfo, SlotInfoNull) {
  ChapsProxyMock proxy(true);
  EXPECT_EQ(CKR_ARGUMENTS_BAD, C_GetSlotInfo(1, NULL));
}

TEST(TestSlotInfo, SlotInfoNotInit) {
  ChapsProxyMock proxy(false);
  CK_SLOT_INFO info;
  EXPECT_EQ(CKR_CRYPTOKI_NOT_INITIALIZED, C_GetSlotInfo(1, &info));
}

TEST(TestSlotInfo, SlotInfoFailure) {
  ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, GetSlotInfo(1, _, _, _, _, _, _, _))
      .WillOnce(Return(CKR_FUNCTION_FAILED));
  CK_SLOT_INFO info;
  EXPECT_EQ(CKR_FUNCTION_FAILED, C_GetSlotInfo(1, &info));
}

// Token Info Tests
TEST(TestTokenInfo, TokenInfoOK) {
  ChapsProxyMock proxy(true);
  CK_TOKEN_INFO info;
  memset(&info, 0, sizeof(info));
  EXPECT_EQ(CKR_OK, C_GetTokenInfo(1, &info));
  EXPECT_EQ(std::string(16, ' '), std::string((char*)info.serialNumber, 16));
  EXPECT_EQ(std::string(32, ' '), std::string((char*)info.manufacturerID, 32));
  EXPECT_EQ(1, info.flags);
}

TEST(TestTokenInfo, TokenInfoNull) {
  ChapsProxyMock proxy(true);
  EXPECT_EQ(CKR_ARGUMENTS_BAD, C_GetTokenInfo(1, NULL));
}

TEST(TestTokenInfo, TokenInfoNotInit) {
  ChapsProxyMock proxy(false);
  CK_TOKEN_INFO info;
  EXPECT_EQ(CKR_CRYPTOKI_NOT_INITIALIZED, C_GetTokenInfo(1, &info));
}

// WaitSlotEvent Tests
TEST(TestWaitSlotEvent, SlotEventNonBlock) {
  ChapsProxyMock proxy(true);
  CK_SLOT_ID slot = 0;
  EXPECT_EQ(CKR_NO_EVENT, C_WaitForSlotEvent(CKF_DONT_BLOCK, &slot, NULL));
}

// This is a helper function for the SlotEventBlock test.
static void* CallFinalize(void*) {
  // The main thread has likely already proceeded into C_WaitForSlotEvent but to
  // increase this chance we'll yield for a bit. The test will pass even in the
  // unlikely event that we hit C_Finalize before the main thread begins
  // waiting.
  usleep(10000);
  C_Finalize(NULL);
  return NULL;
}

TEST(TestWaitSlotEvent, SlotEventBlock) {
  ChapsProxyMock proxy(true);
  CK_SLOT_ID slot = 0;
  pthread_t thread;
  pthread_create(&thread, NULL, CallFinalize, NULL);
  EXPECT_EQ(CKR_CRYPTOKI_NOT_INITIALIZED, C_WaitForSlotEvent(0, &slot, NULL));
}

TEST(TestWaitSlotEvent, SlotEventNotInit) {
  ChapsProxyMock proxy(false);
  CK_SLOT_ID slot = 0;
  EXPECT_EQ(CKR_CRYPTOKI_NOT_INITIALIZED, C_WaitForSlotEvent(0, &slot, NULL));
}

TEST(TestWaitSlotEvent, SlotEventBadArgs) {
  ChapsProxyMock proxy(true);
  EXPECT_EQ(CKR_ARGUMENTS_BAD, C_WaitForSlotEvent(0, NULL, NULL));
}

// Mechanism List Tests
class TestMechList : public ::testing::Test {
protected:
  virtual void SetUp() {
    uint32_t mech_array[3] = {1, 2, 3};
    mech_list_all_.assign(&mech_array[0], &mech_array[3]);
    mech_list_present_.assign(&mech_array[1], &mech_array[3]);
  }
  vector<uint32_t> mech_list_all_;
  vector<uint32_t> mech_list_present_;
};

TEST_F(TestMechList, MechListOK) {
  ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, GetMechanismList(false, _))
      .WillOnce(DoAll(SetArgumentPointee<1>(mech_list_all_),
                      Return(CKR_OK)));
  CK_SLOT_ID mechs[3];
  CK_ULONG num_mechs = 3;
  EXPECT_EQ(CKR_OK, C_GetMechanismList(CK_FALSE, mechs, &num_mechs));
  EXPECT_EQ(num_mechs, mech_list_all_.size());
  EXPECT_EQ(mechs[0], mech_list_all_[0]);
  EXPECT_EQ(mechs[1], mech_list_all_[1]);
  EXPECT_EQ(mechs[2], mech_list_all_[2]);
}

TEST_F(TestMechList, MechListNull) {
  ChapsProxyMock proxy(true);
  EXPECT_EQ(CKR_ARGUMENTS_BAD, C_GetMechanismList(CK_FALSE, NULL, NULL));
}

TEST_F(TestMechList, MechListNotInit) {
  ChapsProxyMock proxy(false);
  CK_SLOT_ID mechs[3];
  CK_ULONG num_mechs = 3;
  EXPECT_EQ(CKR_CRYPTOKI_NOT_INITIALIZED,
            C_GetMechanismList(CK_FALSE, mechs, &num_mechs));
}

TEST_F(TestMechList, MechListNoBuffer) {
  ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, GetMechanismList(false, _))
      .WillOnce(DoAll(SetArgumentPointee<1>(mech_list_all_),
                      Return(CKR_OK)));
  CK_ULONG num_mechs = 17;
  EXPECT_EQ(CKR_OK, C_GetMechanismList(CK_FALSE, NULL, &num_mechs));
  EXPECT_EQ(num_mechs, mech_list_all_.size());
}

TEST_F(TestMechList, MechListSmallBuffer) {
  ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, GetMechanismList(false, _))
      .WillOnce(DoAll(SetArgumentPointee<1>(mech_list_all_),
                      Return(CKR_OK)));
  CK_SLOT_ID mechs[2];
  CK_ULONG num_mechs = 2;
  EXPECT_EQ(CKR_BUFFER_TOO_SMALL, C_GetMechanismList(CK_FALSE, mechs,
                                                     &num_mechs));
  EXPECT_EQ(num_mechs, mech_list_all_.size());
}

TEST_F(TestMechList, MechListLargeBuffer) {
  ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, GetMechanismList(false, _))
      .WillOnce(DoAll(SetArgumentPointee<1>(mech_list_all_),
                      Return(CKR_OK)));
  CK_SLOT_ID mechs[4];
  CK_ULONG num_mechs = 4;
  EXPECT_EQ(CKR_OK, C_GetMechanismList(CK_FALSE, mechs, &num_mechs));
  EXPECT_EQ(num_mechs, mech_list_all_.size());
  EXPECT_EQ(mechs[0], mech_list_all_[0]);
  EXPECT_EQ(mechs[1], mech_list_all_[1]);
  EXPECT_EQ(mechs[2], mech_list_all_[2]);
}

TEST_F(TestMechList, MechListPresentOnly) {
  ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, GetMechanismList(true, _))
      .WillOnce(DoAll(SetArgumentPointee<1>(mech_list_present_),
                      Return(CKR_OK)));
  CK_SLOT_ID mechs[4];
  CK_ULONG num_mechs = 4;
  EXPECT_EQ(CKR_OK, C_GetMechanismList(CK_TRUE, mechs, &num_mechs));
  EXPECT_EQ(num_mechs, mech_list_present_.size());
  EXPECT_EQ(mechs[0], mech_list_present_[0]);
  EXPECT_EQ(mechs[1], mech_list_present_[1]);
}

TEST_F(TestMechList, MechListFailure) {
  ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, GetMechanismList(false, _))
      .WillOnce(DoAll(SetArgumentPointee<1>(mech_list_present_),
                      Return(CKR_FUNCTION_FAILED)));
  CK_SLOT_ID mechs[4];
  CK_ULONG num_mechs = 4;
  EXPECT_EQ(CKR_FUNCTION_FAILED, C_GetMechanismList(CK_FALSE, mechs,
                                                    &num_mechs));
}

// Mechanism Info Tests
TEST(TestMechInfo, MechInfoOK) {
  ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, GetMechanismInfo(1, 2, _, _, _))
      .WillOnce(DoAll(SetArgumentPointee<4>(1), Return(CKR_OK)));
  CK_MECHANISM_INFO info;
  memset(&info, 0, sizeof(info));
  EXPECT_EQ(CKR_OK, C_GetMechanismInfo(1, 2, &info));
  EXPECT_EQ(1, info.flags);
}

TEST(TestMechInfo, MechInfoNull) {
  ChapsProxyMock proxy(true);
  EXPECT_EQ(CKR_ARGUMENTS_BAD, C_GetMechanismInfo(1, 2, NULL));
}

TEST(TestMechInfo, MechInfoNotInit) {
  ChapsProxyMock proxy(false);
  CK_MECHANISM_INFO info;
  EXPECT_EQ(CKR_CRYPTOKI_NOT_INITIALIZED, C_GetMechanismInfo(1, 2, &info));
}

TEST(TestMechInfo, MechInfoFailure) {
  ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, GetMechanismInfo(1, 2, _, _, _))
      .WillOnce(Return(CKR_MECHANISM_INVALID));
  CK_MECHANISM_INFO info;
  EXPECT_EQ(CKR_MECHANISM_INVALID, C_GetMechanismInfo(1, 2, &info));
}

// Init Token Tests
TEST(TestInitToken, InitTokenOK) {
  ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, InitToken(1, _, _))
      .WillOnce(Return(CKR_OK));
  CK_UTF8CHAR_PTR pin = (CK_UTF8CHAR_PTR)"test";
  CK_UTF8CHAR label[32];
  memset(label, ' ', 32);
  memcpy(label, "test", 4);
  EXPECT_EQ(CKR_OK, C_InitToken(1, pin, 4, label));
}

TEST(TestInitToken, InitTokenNotInit) {
  ChapsProxyMock proxy(false);
  CK_UTF8CHAR label[32];
  memset(label, ' ', 32);
  memcpy(label, "test", 4);
  EXPECT_EQ(CKR_CRYPTOKI_NOT_INITIALIZED, C_InitToken(1, NULL, 0, label));
}

TEST(TestInitToken, InitTokenNULLLabel) {
  ChapsProxyMock proxy(true);
  EXPECT_EQ(CKR_ARGUMENTS_BAD, C_InitToken(1, NULL, 0, NULL));
}

TEST(TestInitToken, InitTokenNULLPin) {
  ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, InitToken(1, _, _))
      .WillOnce(Return(CKR_OK));
  CK_UTF8CHAR label[32];
  memset(label, ' ', 32);
  memcpy(label, "test", 4);
  EXPECT_EQ(CKR_OK, C_InitToken(1, NULL, 0, label));
}

TEST(TestInitToken, InitTokenFail) {
  ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, InitToken(1, _, _))
      .WillOnce(Return(CKR_PIN_INVALID));
  CK_UTF8CHAR label[32];
  memset(label, ' ', 32);
  memcpy(label, "test", 4);
  EXPECT_EQ(CKR_PIN_INVALID, C_InitToken(1, NULL, 0, label));
}

// Init PIN Tests
TEST(TestInitPIN, InitPINOK) {
  ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, InitPIN(1, _))
      .WillOnce(Return(CKR_OK));
  CK_UTF8CHAR_PTR pin = (CK_UTF8CHAR_PTR)"test";
  EXPECT_EQ(CKR_OK, C_InitPIN(1, pin, 4));
}

TEST(TestInitPIN, InitPINNotInit) {
  ChapsProxyMock proxy(false);
  EXPECT_EQ(CKR_CRYPTOKI_NOT_INITIALIZED, C_InitPIN(1, NULL, 0));
}

TEST(TestInitPIN, InitPINNULLPin) {
  ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, InitPIN(1, _))
      .WillOnce(Return(CKR_OK));
  EXPECT_EQ(CKR_OK, C_InitPIN(1, NULL, 0));
}

TEST(TestInitPIN, InitPINFail) {
  ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, InitPIN(1, _))
      .WillOnce(Return(CKR_PIN_INVALID));
  EXPECT_EQ(CKR_PIN_INVALID, C_InitPIN(1, NULL, 0));
}

// Set PIN Tests
TEST(TestSetPIN, SetPINOK) {
  ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, SetPIN(1, _, _))
      .WillOnce(Return(CKR_OK));
  CK_UTF8CHAR_PTR pin = (CK_UTF8CHAR_PTR)"test";
  EXPECT_EQ(CKR_OK, C_SetPIN(1, pin, 4, pin, 4));
}

TEST(TestSetPIN, SetPINNotInit) {
  ChapsProxyMock proxy(false);
  EXPECT_EQ(CKR_CRYPTOKI_NOT_INITIALIZED, C_SetPIN(1, NULL, 0, NULL, 0));
}

TEST(TestSetPIN, SetPINNULLPin) {
  ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, SetPIN(1, _, _))
      .WillOnce(Return(CKR_OK));
  EXPECT_EQ(CKR_OK, C_SetPIN(1, NULL, 0, NULL, 0));
}

TEST(TestSetPIN, SetPINFail) {
  ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, SetPIN(1, _, _))
      .WillOnce(Return(CKR_PIN_INVALID));
  EXPECT_EQ(CKR_PIN_INVALID, C_SetPIN(1, NULL, 0, NULL, 0));
}

// Open Session Tests
TEST(TestOpenSession, OpenSessionOK) {
  ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, OpenSession(1, CKF_SERIAL_SESSION, _))
      .WillOnce(DoAll(SetArgumentPointee<2>(3), Return(CKR_OK)));
  CK_SESSION_HANDLE session;
  EXPECT_EQ(CKR_OK, C_OpenSession(1, CKF_SERIAL_SESSION, NULL, NULL, &session));
  EXPECT_EQ(session, 3);
}

TEST(TestOpenSession, OpenSessionNotInit) {
  ChapsProxyMock proxy(false);
  CK_SESSION_HANDLE session;
  EXPECT_EQ(CKR_CRYPTOKI_NOT_INITIALIZED,
            C_OpenSession(1, CKF_SERIAL_SESSION, NULL, NULL, &session));
}

TEST(TestOpenSession, OpenSessionNull) {
  ChapsProxyMock proxy(true);
  EXPECT_EQ(CKR_ARGUMENTS_BAD,
            C_OpenSession(1, CKF_SERIAL_SESSION, NULL, NULL, NULL));
}

TEST(TestOpenSession, OpenSessionFail) {
  ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, OpenSession(1, CKF_SERIAL_SESSION, _))
      .WillOnce(Return(CKR_SESSION_COUNT));
  CK_SESSION_HANDLE session;
  EXPECT_EQ(CKR_SESSION_COUNT,
            C_OpenSession(1, CKF_SERIAL_SESSION, NULL, NULL, &session));
}

// Close Session Tests
TEST(TestCloseSession, CloseSessionOK) {
  ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, CloseSession(1))
      .WillOnce(Return(CKR_OK));
  EXPECT_EQ(CKR_OK, C_CloseSession(1));
}

TEST(TestCloseSession, CloseSessionNotInit) {
  ChapsProxyMock proxy(false);
  EXPECT_EQ(CKR_CRYPTOKI_NOT_INITIALIZED, C_CloseSession(1));
}

TEST(TestCloseSession, CloseSessionFail) {
  ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, CloseSession(1))
      .WillOnce(Return(CKR_SESSION_HANDLE_INVALID));
  EXPECT_EQ(CKR_SESSION_HANDLE_INVALID, C_CloseSession(1));
}

TEST(TestCloseSession, CloseAllSessionsOK) {
  ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, CloseAllSessions(1))
      .WillOnce(Return(CKR_OK));
  EXPECT_EQ(CKR_OK, C_CloseAllSessions(1));
}

TEST(TestCloseSession, CloseAllSessionsNotInit) {
  ChapsProxyMock proxy(false);
  EXPECT_EQ(CKR_CRYPTOKI_NOT_INITIALIZED, C_CloseAllSessions(1));
}

TEST(TestCloseSession, CloseAllSessionsFail) {
  ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, CloseAllSessions(1))
      .WillOnce(Return(CKR_SLOT_ID_INVALID));
  EXPECT_EQ(CKR_SLOT_ID_INVALID, C_CloseAllSessions(1));
}

// Get Session Info Tests
TEST(TestGetSessionInfo, GetSessionInfoOK) {
  ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, GetSessionInfo(1, _, _, _, _))
      .WillOnce(DoAll(SetArgumentPointee<1>(2), Return(CKR_OK)));
  CK_SESSION_INFO info;
  EXPECT_EQ(CKR_OK, C_GetSessionInfo(1, &info));
  EXPECT_EQ(2, info.slotID);
}

TEST(TestGetSessionInfo, GetSessionInfoNotInit) {
  ChapsProxyMock proxy(false);
  CK_SESSION_INFO info;
  EXPECT_EQ(CKR_CRYPTOKI_NOT_INITIALIZED, C_GetSessionInfo(1, &info));
}

TEST(TestGetSessionInfo, GetSessionInfoNull) {
  ChapsProxyMock proxy(true);
  EXPECT_EQ(CKR_ARGUMENTS_BAD, C_GetSessionInfo(1, NULL));
}

TEST(TestGetSessionInfo, GetSessionInfoFail) {
  ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, GetSessionInfo(1, _, _, _, _))
      .WillOnce(Return(CKR_SESSION_HANDLE_INVALID));
  CK_SESSION_INFO info;
  EXPECT_EQ(CKR_SESSION_HANDLE_INVALID, C_GetSessionInfo(1, &info));
}

// Get Operation State Tests
class TestGetOperationState : public ::testing::Test {
protected:
  virtual void SetUp() {
    buffer_ = "123";
  }
  string buffer_;
};

TEST_F(TestGetOperationState, GetOperationStateOK) {
  ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, GetOperationState(1, _))
      .WillOnce(DoAll(SetArgumentPointee<1>(buffer_), Return(CKR_OK)));
  CK_BYTE buffer[3];
  CK_ULONG size = 3;
  EXPECT_EQ(CKR_OK, C_GetOperationState(1, buffer, &size));
  EXPECT_EQ(buffer[0], buffer_[0]);
  EXPECT_EQ(buffer[1], buffer_[1]);
  EXPECT_EQ(buffer[2], buffer_[2]);
}

TEST_F(TestGetOperationState, GetOperationStateNull) {
  ChapsProxyMock proxy(true);
  EXPECT_EQ(CKR_ARGUMENTS_BAD, C_GetOperationState(CK_FALSE, NULL, NULL));
}

TEST_F(TestGetOperationState, GetOperationStateNotInit) {
  ChapsProxyMock proxy(false);
  CK_BYTE buffer[3];
  CK_ULONG size = 3;
  EXPECT_EQ(CKR_CRYPTOKI_NOT_INITIALIZED,
            C_GetOperationState(1, buffer, &size));
}

TEST_F(TestGetOperationState, GetOperationStateNoBuffer) {
  ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, GetOperationState(1, _))
      .WillOnce(DoAll(SetArgumentPointee<1>(buffer_), Return(CKR_OK)));
  CK_ULONG size = 17;
  EXPECT_EQ(CKR_OK, C_GetOperationState(1, NULL, &size));
  EXPECT_EQ(size, buffer_.size());
}

TEST_F(TestGetOperationState, GetOperationStateSmallBuffer) {
  ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, GetOperationState(1, _))
      .WillOnce(DoAll(SetArgumentPointee<1>(buffer_), Return(CKR_OK)));
  CK_BYTE buffer[2];
  CK_ULONG size = 2;
  EXPECT_EQ(CKR_BUFFER_TOO_SMALL, C_GetOperationState(1, buffer, &size));
  EXPECT_EQ(size, buffer_.size());
}

TEST_F(TestGetOperationState, GetOperationStateLargeBuffer) {
  ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, GetOperationState(1, _))
      .WillOnce(DoAll(SetArgumentPointee<1>(buffer_), Return(CKR_OK)));
  CK_BYTE buffer[4];
  CK_ULONG size = 4;
  EXPECT_EQ(CKR_OK, C_GetOperationState(1, buffer, &size));
  EXPECT_EQ(size, buffer_.size());
  EXPECT_EQ(buffer[0], buffer_[0]);
  EXPECT_EQ(buffer[1], buffer_[1]);
  EXPECT_EQ(buffer[2], buffer_[2]);
}

TEST_F(TestGetOperationState, GetOperationStateFailure) {
  ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, GetOperationState(1, _))
      .WillOnce(Return(CKR_STATE_UNSAVEABLE));
  CK_BYTE buffer[3];
  CK_ULONG size = 3;
  EXPECT_EQ(CKR_STATE_UNSAVEABLE, C_GetOperationState(1, buffer, &size));
}

// Set Operation State Tests
TEST(TestSetOperationState, SetOperationStateOK) {
  ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, SetOperationState(1, _, 2, 3))
      .WillOnce(Return(CKR_OK));
  CK_BYTE buffer[3];
  CK_ULONG size = 3;
  EXPECT_EQ(CKR_OK, C_SetOperationState(1, buffer, size, 2, 3));
}

TEST(TestSetOperationState, SetOperationStateNotInit) {
  ChapsProxyMock proxy(false);
  CK_BYTE buffer[3];
  CK_ULONG size = 3;
  EXPECT_EQ(CKR_CRYPTOKI_NOT_INITIALIZED,
            C_SetOperationState(1, buffer, size, 2, 3));
}

TEST(TestSetOperationState, SetOperationStateNull) {
  ChapsProxyMock proxy(true);
  EXPECT_EQ(CKR_ARGUMENTS_BAD, C_SetOperationState(1, NULL, 0, 2, 3));
}

TEST(TestSetOperationState, SetOperationStateFail) {
  ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, SetOperationState(1, _, 2, 3))
      .WillOnce(Return(CKR_SESSION_HANDLE_INVALID));
  CK_BYTE buffer[3];
  CK_ULONG size = 3;
  EXPECT_EQ(CKR_SESSION_HANDLE_INVALID,
            C_SetOperationState(1, buffer, size, 2, 3));
}

// Login Tests
TEST(TestLogin, LoginOK) {
  ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, Login(1, CKU_USER, _))
      .WillOnce(Return(CKR_OK));
  CK_UTF8CHAR_PTR pin = (CK_UTF8CHAR_PTR)"test";
  EXPECT_EQ(CKR_OK, C_Login(1, CKU_USER, pin, 4));
}

TEST(TestLogin, LoginNotInit) {
  ChapsProxyMock proxy(false);
  EXPECT_EQ(CKR_CRYPTOKI_NOT_INITIALIZED, C_Login(1, CKU_USER, NULL, 0));
}

TEST(TestLogin, LoginNULLPin) {
  ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, Login(1, CKU_USER, _))
      .WillOnce(Return(CKR_OK));
  EXPECT_EQ(CKR_OK, C_Login(1, CKU_USER, NULL, 0));
}

TEST(TestLogin, LoginFail) {
  ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, Login(1, CKU_USER, _))
      .WillOnce(Return(CKR_PIN_INVALID));
  EXPECT_EQ(CKR_PIN_INVALID, C_Login(1, CKU_USER, NULL, 0));
}

// Logout Tests
TEST(TestLogout, LogoutOK) {
  ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, Logout(1))
      .WillOnce(Return(CKR_OK));
  EXPECT_EQ(CKR_OK, C_Logout(1));
}

TEST(TestLogout, LogoutNotInit) {
  ChapsProxyMock proxy(false);
  EXPECT_EQ(CKR_CRYPTOKI_NOT_INITIALIZED, C_Logout(1));
}

TEST(TestLogout, LogoutFail) {
  ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, Logout(1))
      .WillOnce(Return(CKR_SESSION_HANDLE_INVALID));
  EXPECT_EQ(CKR_SESSION_HANDLE_INVALID, C_Logout(1));
}

// CreateObject Tests
class TestAttributes : public ::testing::Test {
protected:
  virtual void SetUp() {
    attribute_template_[0].type = CKA_ID;
    attribute_template_[0].ulValueLen = 4;
    attribute_template_[0].pValue = const_cast<char*>("test");
    attribute_template_[1].type = CKA_AC_ISSUER;
    attribute_template_[1].ulValueLen = 5;
    attribute_template_[1].pValue = const_cast<char*>("test2");
    attribute_template2_[0].type = CKA_ID;
    attribute_template2_[0].ulValueLen = 4;
    attribute_template2_[0].pValue = buf_[0];
    attribute_template2_[1].type = CKA_AC_ISSUER;
    attribute_template2_[1].ulValueLen = 5;
    attribute_template2_[1].pValue = buf_[1];
    attribute_template3_[0].type = CKA_ID;
    attribute_template3_[0].ulValueLen = 4;
    attribute_template3_[0].pValue = NULL;
    attribute_template3_[1].type = CKA_AC_ISSUER;
    attribute_template3_[1].ulValueLen = 5;
    attribute_template3_[1].pValue = NULL;
    ASSERT_TRUE(SerializeAttributes(attribute_template_, 2, &attributes_));
    ASSERT_TRUE(SerializeAttributes(attribute_template2_, 2, &attributes2_));
    ASSERT_TRUE(SerializeAttributes(attribute_template3_, 2, &attributes3_));
  }

  bool CompareAttributes(CK_ATTRIBUTE_PTR a1, CK_ATTRIBUTE_PTR a2, int size) {
    if (!a1 || !a2)
      return false;
    for (int i = 0; i < size; ++i) {
      if (a1[i].type != a2[i].type ||
          a1[i].ulValueLen != a2[i].ulValueLen ||
          !a1[i].pValue != !a2[i].pValue)
        return false;
      if (!a1[i].pValue)
        continue;
      if (0 != memcmp(a1[i].pValue, a2[i].pValue, a1[i].ulValueLen))
        return false;
    }
    return true;
  }

  string attributes_;
  string attributes2_;
  string attributes3_;
  CK_ATTRIBUTE attribute_template_[2];
  char buf_[2][10];
  CK_ATTRIBUTE attribute_template2_[2];
  CK_ATTRIBUTE attribute_template3_[2];
};

TEST_F(TestAttributes, CreateObjectOK) {
  ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, CreateObject(1, attributes_, _))
      .WillOnce(DoAll(SetArgumentPointee<2>(3), Return(CKR_OK)));
  CK_OBJECT_HANDLE object_handle = 0;
  EXPECT_EQ(CKR_OK, C_CreateObject(1, attribute_template_, 2, &object_handle));
  EXPECT_EQ(3, object_handle);
}

TEST_F(TestAttributes, CreateObjectNotInit) {
  ChapsProxyMock proxy(false);
  CK_OBJECT_HANDLE object_handle = 0;
  EXPECT_EQ(CKR_CRYPTOKI_NOT_INITIALIZED, C_CreateObject(1,
                                                         attribute_template_, 2,
                                                         &object_handle));
}

TEST_F(TestAttributes, CreateObjectNULLHandle) {
  ChapsProxyMock proxy(true);
  EXPECT_EQ(CKR_ARGUMENTS_BAD, C_CreateObject(1, attribute_template_, 2, NULL));
}

TEST_F(TestAttributes, CreateObjectFail) {
  ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, CreateObject(1, attributes_, _))
      .WillOnce(Return(CKR_ATTRIBUTE_TYPE_INVALID));
  CK_OBJECT_HANDLE object_handle = 0;
  EXPECT_EQ(CKR_ATTRIBUTE_TYPE_INVALID, C_CreateObject(1,
                                                       attribute_template_, 2,
                                                       &object_handle));
}

// CopyObject Tests
TEST_F(TestAttributes, CopyObjectOK) {
  ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, CopyObject(1, 2, attributes_, _))
      .WillOnce(DoAll(SetArgumentPointee<3>(3), Return(CKR_OK)));
  CK_OBJECT_HANDLE object_handle = 0;
  EXPECT_EQ(CKR_OK, C_CopyObject(1, 2, attribute_template_, 2, &object_handle));
  EXPECT_EQ(3, object_handle);
}

TEST_F(TestAttributes, CopyObjectNotInit) {
  ChapsProxyMock proxy(false);
  CK_OBJECT_HANDLE object_handle = 0;
  EXPECT_EQ(CKR_CRYPTOKI_NOT_INITIALIZED, C_CopyObject(1, 2,
                                                       attribute_template_, 2,
                                                       &object_handle));
}

TEST_F(TestAttributes, CopyObjectNULLHandle) {
  ChapsProxyMock proxy(true);
  EXPECT_EQ(CKR_ARGUMENTS_BAD, C_CopyObject(1, 2,
                                            attribute_template_, 2,
                                            NULL));
}

TEST_F(TestAttributes, CopyObjectFail) {
  ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, CopyObject(1, 2, attributes_, _))
      .WillOnce(Return(CKR_ATTRIBUTE_TYPE_INVALID));
  CK_OBJECT_HANDLE object_handle = 0;
  EXPECT_EQ(CKR_ATTRIBUTE_TYPE_INVALID, C_CopyObject(1, 2,
                                                     attribute_template_, 2,
                                                     &object_handle));
}

// Attribute Serialization Tests
TEST_F(TestAttributes, TestAttributesSerialize) {
  string serialized;
  EXPECT_TRUE(SerializeAttributes(attribute_template_, 2, &serialized));
  EXPECT_TRUE(serialized == attributes_);
  Attributes tmp;
  EXPECT_TRUE(tmp.Parse(attributes_));
  EXPECT_TRUE(CompareAttributes(tmp.attributes(),
                                attribute_template_, 2));
  string serialized2;
  EXPECT_TRUE(SerializeAttributes(tmp.attributes(), 2, &serialized2));
  EXPECT_TRUE(attributes_ == serialized2);
  EXPECT_TRUE(tmp.Parse(serialized));
  EXPECT_TRUE(CompareAttributes(attribute_template_, tmp.attributes(), 2));
}

TEST_F(TestAttributes, TestAttributesFill) {
  char buf1[10];
  char buf2[10];
  CK_ATTRIBUTE tmp_array[] = {
    {CKA_ID, buf1, 10},
    {CKA_AC_ISSUER, buf2, 10}
  };
  EXPECT_TRUE(ParseAndFillAttributes(attributes_, tmp_array, 2));
  EXPECT_TRUE(CompareAttributes(attribute_template_, tmp_array, 2));
  EXPECT_FALSE(ParseAndFillAttributes(attributes_, NULL, 2));
  EXPECT_FALSE(ParseAndFillAttributes("invalid_string", tmp_array, 2));
  EXPECT_FALSE(ParseAndFillAttributes(attributes_, tmp_array, 1));
  EXPECT_FALSE(ParseAndFillAttributes(attributes_, tmp_array, 3));
  tmp_array[0].pValue = NULL;
  EXPECT_FALSE(ParseAndFillAttributes(attributes_, tmp_array, 2));
  tmp_array[0].pValue = buf1;
  tmp_array[0].ulValueLen = 1;
  EXPECT_FALSE(ParseAndFillAttributes(attributes_, tmp_array, 2));
}

TEST_F(TestAttributes, TestAttributesNested) {
  char id[] = "myid";
  char issuer[] = "myissuer";
  CK_BBOOL true_val = CK_TRUE;
  CK_ATTRIBUTE tmp_array_inner[] = {
    {CKA_ENCRYPT, &true_val, sizeof(CK_BBOOL)},
    {CKA_SIGN, &true_val, sizeof(CK_BBOOL)}
  };
  CK_ATTRIBUTE tmp_array[] = {
    {CKA_ID, id, 4},
    {CKA_AC_ISSUER, issuer, 8},
    {CKA_WRAP_TEMPLATE, tmp_array_inner, sizeof(tmp_array_inner)}
  };
  string serialized;
  EXPECT_TRUE(SerializeAttributes(tmp_array, 3, &serialized));
  Attributes parsed;
  EXPECT_TRUE(parsed.Parse(serialized));
  EXPECT_TRUE(CompareAttributes(parsed.attributes(), tmp_array, 2));
  EXPECT_TRUE(CompareAttributes((CK_ATTRIBUTE_PTR)parsed.attributes()[2].pValue,
                                tmp_array_inner, 2));
  // Test a nested parse-and-fill.
  CK_BBOOL val1, val2;
  char buf1[10];
  char buf2[10];
  CK_ATTRIBUTE tmp_array_inner2[] = {
    {CKA_ENCRYPT, &val1, sizeof(CK_BBOOL)},
    {CKA_SIGN, &val2, sizeof(CK_BBOOL)}
  };
  CK_ATTRIBUTE tmp_array2[] = {
    {CKA_ID, buf1, 10},
    {CKA_AC_ISSUER, buf2, 10},
    {CKA_WRAP_TEMPLATE, tmp_array_inner2, sizeof(tmp_array_inner2)}
  };
  EXPECT_TRUE(ParseAndFillAttributes(serialized, tmp_array2, 3));
  EXPECT_TRUE(CompareAttributes(tmp_array2, tmp_array, 2));
  EXPECT_TRUE(CompareAttributes(tmp_array_inner2, tmp_array_inner, 2));
  // Test circular nesting.
  tmp_array[2].pValue = tmp_array;
  tmp_array[2].ulValueLen = sizeof(tmp_array);
  EXPECT_FALSE(SerializeAttributes(tmp_array, 3, &serialized));
}

// DestroyObject Tests
TEST(TestDestroyObject, DestroyObjectOK) {
  ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, DestroyObject(1, 2))
      .WillOnce(Return(CKR_OK));
  EXPECT_EQ(CKR_OK, C_DestroyObject(1, 2));
}

TEST(TestDestroyObject, DestroyObjectNotInit) {
  ChapsProxyMock proxy(false);
  EXPECT_EQ(CKR_CRYPTOKI_NOT_INITIALIZED, C_DestroyObject(1, 2));
}

TEST(TestDestroyObject, DestroyObjectFail) {
  ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, DestroyObject(1, 2))
      .WillOnce(Return(CKR_OBJECT_HANDLE_INVALID));
  EXPECT_EQ(CKR_OBJECT_HANDLE_INVALID, C_DestroyObject(1, 2));
}

// GetObjectSize Tests
TEST(TestObjectSize, ObjectSizeOK) {
  ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, GetObjectSize(1, 2, _))
      .WillOnce(DoAll(SetArgumentPointee<2>(20), Return(CKR_OK)));
  CK_ULONG size = 0;
  EXPECT_EQ(CKR_OK, C_GetObjectSize(1, 2, &size));
  EXPECT_EQ(size, 20);
}

TEST(TestObjectSize, ObjectSizeNULL) {
  ChapsProxyMock proxy(true);
  EXPECT_EQ(CKR_ARGUMENTS_BAD, C_GetObjectSize(1, 2, NULL));
}

TEST(TestObjectSize, ObjectSizeNotInit) {
  ChapsProxyMock proxy(false);
  CK_ULONG size = 0;
  EXPECT_EQ(CKR_CRYPTOKI_NOT_INITIALIZED, C_GetObjectSize(1, 2, &size));
}

TEST(TestObjectSize, ObjectSizeFail) {
  ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, GetObjectSize(1, 2, _))
      .WillOnce(Return(CKR_OBJECT_HANDLE_INVALID));
  CK_ULONG size = 0;
  EXPECT_EQ(CKR_OBJECT_HANDLE_INVALID, C_GetObjectSize(1, 2, &size));
}

// GetAttributeValue Tests
TEST_F(TestAttributes, GetAttributeValueOK) {
  ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, GetAttributeValue(1, 2, attributes2_, _))
      .WillOnce(DoAll(SetArgumentPointee<3>(attributes_), Return(CKR_OK)));
  EXPECT_EQ(CKR_OK, C_GetAttributeValue(1, 2, attribute_template2_, 2));
  EXPECT_TRUE(CompareAttributes(attribute_template2_, attribute_template_, 2));
}

TEST_F(TestAttributes, GetAttributeValueSizeOnly) {
  ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, GetAttributeValue(1, 2, _, _))
      .WillOnce(DoAll(SetArgumentPointee<3>(attributes3_), Return(CKR_OK)));
  attribute_template3_[0].ulValueLen = 0;
  attribute_template3_[1].ulValueLen = 0;
  EXPECT_EQ(CKR_OK, C_GetAttributeValue(1, 2, attribute_template3_, 2));
  EXPECT_EQ(4, attribute_template3_[0].ulValueLen);
  EXPECT_EQ(5, attribute_template3_[1].ulValueLen);
}

TEST_F(TestAttributes, GetAttributeValueOKWithError) {
  ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, GetAttributeValue(1, 2, attributes2_, _))
      .WillOnce(DoAll(SetArgumentPointee<3>(attributes_),
                      Return(CKR_ATTRIBUTE_SENSITIVE)));
  EXPECT_EQ(CKR_ATTRIBUTE_SENSITIVE,
      C_GetAttributeValue(1, 2, attribute_template2_, 2));
  EXPECT_TRUE(CompareAttributes(attribute_template2_, attribute_template_, 2));
}

TEST_F(TestAttributes, GetAttributeValueNotInit) {
  ChapsProxyMock proxy(false);
  EXPECT_EQ(CKR_CRYPTOKI_NOT_INITIALIZED,
      C_GetAttributeValue(1, 2, attribute_template3_, 2));
}

TEST_F(TestAttributes, GetAttributeValueNULL) {
  ChapsProxyMock proxy(true);
  EXPECT_EQ(CKR_ARGUMENTS_BAD, C_GetAttributeValue(1, 2, NULL, 2));
}

TEST_F(TestAttributes, GetAttributeValueFail) {
  ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, GetAttributeValue(1, 2, _, _))
      .WillOnce(Return(CKR_OBJECT_HANDLE_INVALID));
  EXPECT_EQ(CKR_OBJECT_HANDLE_INVALID,
      C_GetAttributeValue(1, 2, attribute_template2_, 2));
}

TEST(GetAttributeValueDeathTest, GetAttributeValueFailFatal) {
  ChapsProxyMock proxy(true);
  string invalid("invalid_string");
  ON_CALL(proxy, GetAttributeValue(1, 2, _, _))
      .WillByDefault(DoAll(SetArgumentPointee<3>(invalid), Return(CKR_OK)));
  CK_ATTRIBUTE tmp;
  memset(&tmp, 0, sizeof(tmp));
  EXPECT_DEATH_IF_SUPPORTED(C_GetAttributeValue(1, 2, &tmp, 1), "Check failed");
}

// SetAttributeValue Tests
TEST_F(TestAttributes, SetAttributeValueOK) {
  ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, SetAttributeValue(1, 2, attributes_))
      .WillOnce(Return(CKR_OK));
  EXPECT_EQ(CKR_OK, C_SetAttributeValue(1, 2, attribute_template_, 2));
}

TEST_F(TestAttributes, SetAttributeValueNotInit) {
  ChapsProxyMock proxy(false);
  EXPECT_EQ(CKR_CRYPTOKI_NOT_INITIALIZED,
      C_SetAttributeValue(1, 2, attribute_template_, 2));
}

TEST_F(TestAttributes, SetAttributeValueNULL) {
  ChapsProxyMock proxy(true);
  EXPECT_EQ(CKR_ARGUMENTS_BAD, C_SetAttributeValue(1, 2, NULL, 2));
}

TEST_F(TestAttributes, SetAttributeValueFail) {
  ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, SetAttributeValue(1, 2, _))
      .WillOnce(Return(CKR_OBJECT_HANDLE_INVALID));
  EXPECT_EQ(CKR_OBJECT_HANDLE_INVALID,
      C_SetAttributeValue(1, 2, attribute_template2_, 2));
}

}  // namespace chaps

int main(int argc, char** argv) {
  ::testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}
