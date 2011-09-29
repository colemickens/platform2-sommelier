// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Chaps client unit tests. These tests exercise the client layer (chaps.cc) and
// use a mock for the proxy interface so no D-Bus code is run.
//

#include "chaps/chaps_proxy_mock.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "pkcs11/cryptoki.h"

using std::string;
using std::vector;
using ::testing::_;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::Return;
using ::testing::SetArgumentPointee;

// Initialize / Finalize tests
TEST(TestInitialize,InitializeNULL) {
  chaps::ChapsProxyMock proxy(false);
  EXPECT_EQ(CKR_OK, C_Initialize(NULL_PTR));
  EXPECT_EQ(CKR_OK, C_Finalize(NULL_PTR));
}

TEST(TestInitialize,InitializeOutOfMem) {
  chaps::EnableMockProxy(NULL, false);
  EXPECT_EQ(CKR_HOST_MEMORY, C_Initialize(NULL_PTR));
  chaps::DisableMockProxy();
}

TEST(TestInitialize,InitializeTwice) {
  chaps::ChapsProxyMock proxy(false);
  EXPECT_EQ(CKR_OK, C_Initialize(NULL_PTR));
  EXPECT_EQ(CKR_CRYPTOKI_ALREADY_INITIALIZED, C_Initialize(NULL_PTR));
  EXPECT_EQ(CKR_OK, C_Finalize(NULL_PTR));
}

TEST(TestInitialize,InitializeWithArgs) {
  chaps::ChapsProxyMock proxy(false);
  CK_C_INITIALIZE_ARGS args;
  memset(&args, 0, sizeof(args));
  EXPECT_EQ(CKR_OK, C_Initialize(&args));
  EXPECT_EQ(CKR_OK, C_Finalize(NULL_PTR));
}

TEST(TestInitialize,InitializeWithBadArgs) {
  chaps::ChapsProxyMock proxy(false);
  CK_C_INITIALIZE_ARGS args;
  memset(&args, 0, sizeof(args));
  args.CreateMutex = (CK_CREATEMUTEX)1;
  EXPECT_EQ(CKR_ARGUMENTS_BAD, C_Initialize(&args));
  memset(&args, 0, sizeof(args));
  args.pReserved = (CK_VOID_PTR)1;
  EXPECT_EQ(CKR_ARGUMENTS_BAD, C_Initialize(&args));
}

TEST(TestInitialize,InitializeNoLocking) {
  chaps::ChapsProxyMock proxy(false);
  CK_C_INITIALIZE_ARGS args;
  memset(&args, 0xFF, sizeof(args));
  args.flags = 0;
  args.pReserved = 0;
  EXPECT_EQ(CKR_CANT_LOCK, C_Initialize(&args));
}

TEST(TestInitialize,FinalizeWithArgs) {
  EXPECT_EQ(CKR_ARGUMENTS_BAD, C_Finalize((void*)1));
}

TEST(TestInitialize,FinalizeNotInit) {
  EXPECT_EQ(CKR_CRYPTOKI_NOT_INITIALIZED, C_Finalize(NULL_PTR));
}

TEST(TestInitialize,Reinitialize) {
  chaps::ChapsProxyMock proxy(false);
  EXPECT_EQ(CKR_OK, C_Initialize(NULL_PTR));
  EXPECT_EQ(CKR_OK, C_Finalize(NULL_PTR));
  EXPECT_EQ(CKR_OK, C_Initialize(NULL_PTR));
}

// Library Information Tests
TEST(TestLibInfo,LibInfoOK) {
  chaps::ChapsProxyMock proxy(true);
  CK_INFO info;
  EXPECT_EQ(CKR_OK, C_GetInfo(&info));
}

TEST(TestLibInfo,LibInfoNull) {
  chaps::ChapsProxyMock proxy(true);
  EXPECT_EQ(CKR_ARGUMENTS_BAD, C_GetInfo(NULL));
}

TEST(TestLibInfo,LibInfoNotInit) {
  chaps::ChapsProxyMock proxy(false);
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

TEST_F(TestSlotList,SlotListOK) {
  chaps::ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, GetSlotList(Eq(false),_))
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

TEST_F(TestSlotList,SlotListNull) {
  chaps::ChapsProxyMock proxy(true);
  EXPECT_EQ(CKR_ARGUMENTS_BAD, C_GetSlotList(CK_FALSE, NULL, NULL));
}

TEST_F(TestSlotList,SlotListNotInit) {
  chaps::ChapsProxyMock proxy(false);
  CK_SLOT_ID slots[3];
  CK_ULONG num_slots = 3;
  EXPECT_EQ(CKR_CRYPTOKI_NOT_INITIALIZED,
            C_GetSlotList(CK_FALSE, slots, &num_slots));
}

TEST_F(TestSlotList,SlotListNoBuffer) {
  chaps::ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, GetSlotList(Eq(false),_))
      .WillOnce(DoAll(SetArgumentPointee<1>(slot_list_all_),
                      Return(CKR_OK)));

  CK_ULONG num_slots = 17;
  EXPECT_EQ(CKR_OK, C_GetSlotList(CK_FALSE, NULL, &num_slots));
  EXPECT_EQ(num_slots, slot_list_all_.size());
}

TEST_F(TestSlotList,SlotListSmallBuffer) {
  chaps::ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, GetSlotList(Eq(false),_))
      .WillOnce(DoAll(SetArgumentPointee<1>(slot_list_all_),
                      Return(CKR_OK)));

  CK_SLOT_ID slots[2];
  CK_ULONG num_slots = 2;
  EXPECT_EQ(CKR_BUFFER_TOO_SMALL, C_GetSlotList(CK_FALSE, slots, &num_slots));
  EXPECT_EQ(num_slots, slot_list_all_.size());
}

TEST_F(TestSlotList,SlotListLargeBuffer) {
  chaps::ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, GetSlotList(Eq(false),_))
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

TEST_F(TestSlotList,SlotListPresentOnly) {
  chaps::ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, GetSlotList(Eq(true),_))
      .WillOnce(DoAll(SetArgumentPointee<1>(slot_list_present_),
                      Return(CKR_OK)));

  CK_SLOT_ID slots[4];
  CK_ULONG num_slots = 4;
  EXPECT_EQ(CKR_OK, C_GetSlotList(CK_TRUE, slots, &num_slots));
  EXPECT_EQ(num_slots, slot_list_present_.size());
  EXPECT_EQ(slots[0], slot_list_present_[0]);
  EXPECT_EQ(slots[1], slot_list_present_[1]);
}

TEST_F(TestSlotList,SlotListFailure) {
  chaps::ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, GetSlotList(Eq(false),_))
      .WillOnce(DoAll(SetArgumentPointee<1>(slot_list_present_),
                      Return(CKR_FUNCTION_FAILED)));

  CK_SLOT_ID slots[4];
  CK_ULONG num_slots = 4;
  EXPECT_EQ(CKR_FUNCTION_FAILED, C_GetSlotList(CK_FALSE, slots, &num_slots));
}

// Slot Info Tests
TEST(TestSlotInfo,SlotInfoOK) {
  chaps::ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, GetSlotInfo(Eq(1),_,_,_,_,_,_,_))
      .WillOnce(DoAll(SetArgumentPointee<3>(1), Return(CKR_OK)));

  CK_SLOT_INFO info;
  memset(&info, 0, sizeof(info));
  EXPECT_EQ(CKR_OK, C_GetSlotInfo(1, &info));
  EXPECT_EQ(string(64, ' '), string((char*)info.slotDescription, 64));
  EXPECT_EQ(string(32, ' '), string((char*)info.manufacturerID, 32));
  EXPECT_EQ(1, info.flags);
}

TEST(TestSlotInfo,SlotInfoNull) {
  chaps::ChapsProxyMock proxy(true);
  EXPECT_EQ(CKR_ARGUMENTS_BAD, C_GetSlotInfo(1, NULL));
}

TEST(TestSlotInfo,SlotInfoNotInit) {
  chaps::ChapsProxyMock proxy(false);
  CK_SLOT_INFO info;
  EXPECT_EQ(CKR_CRYPTOKI_NOT_INITIALIZED, C_GetSlotInfo(1, &info));
}

TEST(TestSlotInfo,SlotInfoFailure) {
  chaps::ChapsProxyMock proxy(true);
  EXPECT_CALL(proxy, GetSlotInfo(Eq(1),_,_,_,_,_,_,_))
      .WillOnce(Return(CKR_FUNCTION_FAILED));

  CK_SLOT_INFO info;
  EXPECT_EQ(CKR_FUNCTION_FAILED, C_GetSlotInfo(1, &info));
}

// Token Info Tests
TEST(TestTokenInfo,TokenInfoOK) {
  chaps::ChapsProxyMock proxy(true);
  CK_TOKEN_INFO info;
  memset(&info, 0, sizeof(info));
  EXPECT_EQ(CKR_OK, C_GetTokenInfo(1, &info));
  EXPECT_EQ(std::string(16, ' '), std::string((char*)info.serialNumber, 16));
  EXPECT_EQ(std::string(32, ' '), std::string((char*)info.manufacturerID, 32));
  EXPECT_EQ(1, info.flags);
}

TEST(TestTokenInfo,TokenInfoNull) {
  chaps::ChapsProxyMock proxy(true);
  EXPECT_EQ(CKR_ARGUMENTS_BAD, C_GetTokenInfo(1, NULL));
}

TEST(TestTokenInfo,TokenInfoNotInit) {
  chaps::ChapsProxyMock proxy(false);
  CK_TOKEN_INFO info;
  EXPECT_EQ(CKR_CRYPTOKI_NOT_INITIALIZED, C_GetTokenInfo(1, &info));
}

// WaitSlotEvent Tests
TEST(TestWaitSlotEvent,SlotEventNonBlock) {
  chaps::ChapsProxyMock proxy(true);
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

TEST(TestWaitSlotEvent,SlotEventBlock) {
  chaps::ChapsProxyMock proxy(true);
  CK_SLOT_ID slot = 0;
  pthread_t thread;
  pthread_create(&thread, NULL, CallFinalize, NULL);
  EXPECT_EQ(CKR_CRYPTOKI_NOT_INITIALIZED, C_WaitForSlotEvent(0, &slot, NULL));
}

TEST(TestWaitSlotEvent,SlotEventNotInit) {
  chaps::ChapsProxyMock proxy(false);
  CK_SLOT_ID slot = 0;
  EXPECT_EQ(CKR_CRYPTOKI_NOT_INITIALIZED, C_WaitForSlotEvent(0, &slot, NULL));
}

TEST(TestWaitSlotEvent,SlotEventBadArgs) {
  chaps::ChapsProxyMock proxy(true);
  EXPECT_EQ(CKR_ARGUMENTS_BAD, C_WaitForSlotEvent(0, NULL, NULL));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}
