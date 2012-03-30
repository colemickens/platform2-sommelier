// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chaps_service.h"

#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "chaps_service.h"
#include "object_mock.h"
#include "session_mock.h"
#include "slot_manager_mock.h"

using std::string;
using std::vector;
using ::testing::_;
using ::testing::Return;
using ::testing::SetArgumentPointee;

namespace chaps {

// Invalid initialization test.
TEST(InitDeathTest, InvalidInit) {
  ChapsServiceImpl service(NULL);
  EXPECT_DEATH_IF_SUPPORTED(service.Init(), "Check failed");
}

// Test fixture for an initialized service instance.
class TestService : public ::testing::Test {
protected:
  virtual void SetUp() {
    service_.reset(new ChapsServiceImpl(&slot_manager_));
    ASSERT_TRUE(service_->Init());
    // Setup parsable and un-parsable serialized attributes.
    CK_ATTRIBUTE attributes[] = {{CKA_VALUE, NULL, 0}};
    CK_ATTRIBUTE attributes2[] = {{CKA_VALUE, (void*)"test", 4}};
    attribute_ = attributes[0];
    attribute2_ = attributes2[0];
    Attributes tmp(attributes, 1);
    tmp.Serialize(&good_attributes_);
    Attributes tmp2(attributes2, 1);
    tmp2.Serialize(&good_attributes2_);
    bad_attributes_ = vector<uint8_t>(100, 0xAA);
  }
  virtual void TearDown() {
    service_->TearDown();
  }
  SlotManagerMock slot_manager_;
  SessionMock session_;
  ObjectMock object_;
  scoped_ptr<ChapsServiceImpl> service_;
  vector<uint8_t> bad_attributes_;
  vector<uint8_t> good_attributes_;
  vector<uint8_t> good_attributes2_;
  CK_ATTRIBUTE attribute_;
  CK_ATTRIBUTE attribute2_;
};

TEST_F(TestService, GetSlotList) {
  EXPECT_CALL(slot_manager_, GetSlotCount())
    .WillRepeatedly(Return(2));
  EXPECT_CALL(slot_manager_, IsTokenPresent(_))
    .WillRepeatedly(Return(false));
  // Try bad arguments.
  EXPECT_EQ(CKR_ARGUMENTS_BAD, service_->GetSlotList(false, NULL));
  vector<uint64_t> slot_list;
  slot_list.push_back(0);
  EXPECT_EQ(CKR_ARGUMENTS_BAD, service_->GetSlotList(false, &slot_list));
  // Try normal use cases.
  slot_list.clear();
  EXPECT_EQ(CKR_OK, service_->GetSlotList(true, &slot_list));
  EXPECT_EQ(0, slot_list.size());
  EXPECT_EQ(CKR_OK, service_->GetSlotList(false, &slot_list));
  EXPECT_EQ(2, slot_list.size());
}

TEST_F(TestService, GetSlotInfo) {
  CK_SLOT_INFO test_info;
  memset(&test_info, 0, sizeof(CK_SLOT_INFO));
  test_info.flags = 17;
  EXPECT_CALL(slot_manager_, GetSlotCount())
    .WillRepeatedly(Return(2));
  EXPECT_CALL(slot_manager_, GetSlotInfo(0, _))
    .WillRepeatedly(SetArgumentPointee<1>(test_info));

  // Try bad arguments.
  void* p[7];
  for (int i = 0; i < 7; ++i) {
    memset(p, 1, sizeof(p));
    p[i] = NULL;
    EXPECT_EQ(CKR_ARGUMENTS_BAD, service_->GetSlotInfo(
        0,
        reinterpret_cast<vector<uint8_t>*>(p[0]),
        reinterpret_cast<vector<uint8_t>*>(p[1]),
        reinterpret_cast<uint64_t*>(p[2]),
        reinterpret_cast<uint8_t*>(p[3]),
        reinterpret_cast<uint8_t*>(p[4]),
        reinterpret_cast<uint8_t*>(p[5]),
        reinterpret_cast<uint8_t*>(p[6])));
  }
  vector<uint8_t> slot_description;
  vector<uint8_t> manufacturer_id;
  uint64_t flags;
  uint8_t hardware_version_major;
  uint8_t hardware_version_minor;
  uint8_t firmware_version_major;
  uint8_t firmware_version_minor;
  // Try invalid slot ID.
  EXPECT_EQ(CKR_SLOT_ID_INVALID,
            service_->GetSlotInfo(2,
                                  &slot_description,
                                  &manufacturer_id,
                                  &flags,
                                  &hardware_version_major,
                                  &hardware_version_minor,
                                  &firmware_version_major,
                                  &firmware_version_minor));
  // Try the normal case.
  EXPECT_EQ(CKR_OK, service_->GetSlotInfo(0,
                                          &slot_description,
                                          &manufacturer_id,
                                          &flags,
                                          &hardware_version_major,
                                          &hardware_version_minor,
                                          &firmware_version_major,
                                          &firmware_version_minor));
  EXPECT_EQ(flags, 17);
}

TEST_F(TestService, GetTokenInfo) {
  CK_TOKEN_INFO test_info;
  memset(&test_info, 0, sizeof(CK_TOKEN_INFO));
  test_info.flags = 17;
  EXPECT_CALL(slot_manager_, GetSlotCount())
    .WillRepeatedly(Return(2));
  EXPECT_CALL(slot_manager_, IsTokenPresent(_))
    .WillOnce(Return(false))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(slot_manager_, GetTokenInfo(0, _))
    .WillRepeatedly(SetArgumentPointee<1>(test_info));

  // Try bad arguments.
  void* p[19];
  for (int i = 0; i < 19; ++i) {
    memset(p, 1, sizeof(p));
    p[i] = NULL;
    EXPECT_EQ(CKR_ARGUMENTS_BAD, service_->GetTokenInfo(
        0,
        reinterpret_cast<vector<uint8_t>*>(p[0]),
        reinterpret_cast<vector<uint8_t>*>(p[1]),
        reinterpret_cast<vector<uint8_t>*>(p[2]),
        reinterpret_cast<vector<uint8_t>*>(p[3]),
        reinterpret_cast<uint64_t*>(p[4]),
        reinterpret_cast<uint64_t*>(p[5]),
        reinterpret_cast<uint64_t*>(p[6]),
        reinterpret_cast<uint64_t*>(p[7]),
        reinterpret_cast<uint64_t*>(p[8]),
        reinterpret_cast<uint64_t*>(p[9]),
        reinterpret_cast<uint64_t*>(p[10]),
        reinterpret_cast<uint64_t*>(p[11]),
        reinterpret_cast<uint64_t*>(p[12]),
        reinterpret_cast<uint64_t*>(p[13]),
        reinterpret_cast<uint64_t*>(p[14]),
        reinterpret_cast<uint8_t*>(p[15]),
        reinterpret_cast<uint8_t*>(p[16]),
        reinterpret_cast<uint8_t*>(p[17]),
        reinterpret_cast<uint8_t*>(p[18])));
  }
  vector<uint8_t> label;
  vector<uint8_t> manufacturer_id;
  vector<uint8_t> model;
  vector<uint8_t> serial_number;
  uint64_t flags;
  uint64_t max_session_count;
  uint64_t session_count;
  uint64_t max_session_count_rw;
  uint64_t session_count_rw;
  uint64_t max_pin_len;
  uint64_t min_pin_len;
  uint64_t total_public_memory;
  uint64_t free_public_memory;
  uint64_t total_private_memory;
  uint64_t free_private_memory;
  uint8_t hardware_version_major;
  uint8_t hardware_version_minor;
  uint8_t firmware_version_major;
  uint8_t firmware_version_minor;
  // Try invalid slot ID.
  EXPECT_EQ(CKR_SLOT_ID_INVALID,
            service_->GetTokenInfo(3,
                                   &label,
                                   &manufacturer_id,
                                   &model,
                                   &serial_number,
                                   &flags,
                                   &max_session_count,
                                   &session_count,
                                   &max_session_count_rw,
                                   &session_count_rw,
                                   &max_pin_len,
                                   &min_pin_len,
                                   &total_public_memory,
                                   &free_public_memory,
                                   &total_private_memory,
                                   &free_private_memory,
                                   &hardware_version_major,
                                   &hardware_version_minor,
                                   &firmware_version_major,
                                   &firmware_version_minor));
  EXPECT_EQ(CKR_TOKEN_NOT_PRESENT,
            service_->GetTokenInfo(0,
                                   &label,
                                   &manufacturer_id,
                                   &model,
                                   &serial_number,
                                   &flags,
                                   &max_session_count,
                                   &session_count,
                                   &max_session_count_rw,
                                   &session_count_rw,
                                   &max_pin_len,
                                   &min_pin_len,
                                   &total_public_memory,
                                   &free_public_memory,
                                   &total_private_memory,
                                   &free_private_memory,
                                   &hardware_version_major,
                                   &hardware_version_minor,
                                   &firmware_version_major,
                                   &firmware_version_minor));
  // Try the normal case.
  EXPECT_EQ(CKR_OK, service_->GetTokenInfo(0,
                                           &label,
                                           &manufacturer_id,
                                           &model,
                                           &serial_number,
                                           &flags,
                                           &max_session_count,
                                           &session_count,
                                           &max_session_count_rw,
                                           &session_count_rw,
                                           &max_pin_len,
                                           &min_pin_len,
                                           &total_public_memory,
                                           &free_public_memory,
                                           &total_private_memory,
                                           &free_private_memory,
                                           &hardware_version_major,
                                           &hardware_version_minor,
                                           &firmware_version_major,
                                           &firmware_version_minor));
  EXPECT_EQ(flags, 17);
}

TEST_F(TestService, GetMechanismList) {
  MechanismMap test_list;
  CK_MECHANISM_INFO test_info;
  memset(&test_info, 0, sizeof(CK_MECHANISM_INFO));
  test_list[123UL] = test_info;
  EXPECT_CALL(slot_manager_, GetSlotCount())
    .WillRepeatedly(Return(2));
  EXPECT_CALL(slot_manager_, IsTokenPresent(_))
    .WillOnce(Return(false))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(slot_manager_, GetMechanismInfo(0))
    .WillRepeatedly(Return(&test_list));
  // Try bad arguments.
  EXPECT_EQ(CKR_ARGUMENTS_BAD, service_->GetMechanismList(0, NULL));
  // Try invalid slot ID.
  vector<uint64_t> output;
  EXPECT_EQ(CKR_SLOT_ID_INVALID, service_->GetMechanismList(2, &output));
  EXPECT_EQ(CKR_TOKEN_NOT_PRESENT, service_->GetMechanismList(0, &output));
  // Try the normal case.
  ASSERT_EQ(CKR_OK, service_->GetMechanismList(0, &output));
  ASSERT_EQ(output.size(), 1);
  EXPECT_EQ(output[0], 123);
}

TEST_F(TestService, GetMechanismInfo) {
  MechanismMap test_list;
  CK_MECHANISM_INFO test_info;
  memset(&test_info, 0, sizeof(CK_MECHANISM_INFO));
  test_info.flags = 17;
  test_list[123UL] = test_info;
  EXPECT_CALL(slot_manager_, GetSlotCount())
    .WillRepeatedly(Return(2));
  EXPECT_CALL(slot_manager_, IsTokenPresent(_))
    .WillOnce(Return(false))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(slot_manager_, GetMechanismInfo(0))
    .WillRepeatedly(Return(&test_list));
  uint64_t min_key, max_key, flags;
  // Try bad arguments.
  EXPECT_EQ(CKR_ARGUMENTS_BAD,
            service_->GetMechanismInfo(0, 123, NULL, &max_key, &flags));
  EXPECT_EQ(CKR_ARGUMENTS_BAD,
            service_->GetMechanismInfo(0, 123, &min_key, NULL, &flags));
  EXPECT_EQ(CKR_ARGUMENTS_BAD,
            service_->GetMechanismInfo(0, 123, &min_key, &max_key, NULL));
  // Try invalid slot ID.
  EXPECT_EQ(CKR_SLOT_ID_INVALID,
            service_->GetMechanismInfo(2, 123, &min_key, &max_key, &flags));
  EXPECT_EQ(CKR_TOKEN_NOT_PRESENT,
            service_->GetMechanismInfo(0, 123, &min_key, &max_key, &flags));
  // Try the normal case.
  ASSERT_EQ(CKR_OK,
            service_->GetMechanismInfo(0, 123, &min_key, &max_key, &flags));
  EXPECT_EQ(flags, 17);
}

TEST_F(TestService, InitToken) {
  EXPECT_CALL(slot_manager_, GetSlotCount())
    .WillRepeatedly(Return(2));
  EXPECT_CALL(slot_manager_, IsTokenPresent(_))
    .WillOnce(Return(false))
    .WillOnce(Return(true));
  vector<uint8_t> bad_label;
  vector<uint8_t> good_label(chaps::kTokenLabelSize, 0x20);
  EXPECT_EQ(CKR_ARGUMENTS_BAD, service_->InitToken(0, NULL, bad_label));
  EXPECT_EQ(CKR_SLOT_ID_INVALID, service_->InitToken(2, NULL, good_label));
  EXPECT_EQ(CKR_TOKEN_NOT_PRESENT, service_->InitToken(0, NULL, good_label));
  EXPECT_EQ(CKR_PIN_INCORRECT, service_->InitToken(0, NULL, good_label));
}

TEST_F(TestService, InitPIN) {
  EXPECT_CALL(slot_manager_, GetSession(0, _))
    .WillOnce(Return(false))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(&session_), Return(true)));
  EXPECT_EQ(CKR_SESSION_HANDLE_INVALID, service_->InitPIN(0, NULL));
  EXPECT_EQ(CKR_USER_NOT_LOGGED_IN, service_->InitPIN(0, NULL));
}

TEST_F(TestService, SetPIN) {
  EXPECT_CALL(slot_manager_, GetSession(0, _))
    .WillOnce(Return(false))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(&session_), Return(true)));
  EXPECT_EQ(CKR_SESSION_HANDLE_INVALID, service_->SetPIN(0, NULL, NULL));
  EXPECT_EQ(CKR_PIN_INVALID, service_->SetPIN(0, NULL, NULL));
}

TEST_F(TestService, OpenSession) {
  EXPECT_CALL(slot_manager_, GetSlotCount())
    .WillRepeatedly(Return(2));
  EXPECT_CALL(slot_manager_, IsTokenPresent(_))
    .WillOnce(Return(false))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(slot_manager_, OpenSession(0, true))
    .WillRepeatedly(Return(10));
  EXPECT_EQ(CKR_ARGUMENTS_BAD, service_->OpenSession(0, 0, NULL));
  uint64_t session;
  EXPECT_EQ(CKR_SLOT_ID_INVALID, service_->OpenSession(2, 0, &session));
  EXPECT_EQ(CKR_TOKEN_NOT_PRESENT, service_->OpenSession(0, 0, &session));
  EXPECT_EQ(CKR_SESSION_PARALLEL_NOT_SUPPORTED,
            service_->OpenSession(0, 0, &session));
  ASSERT_EQ(CKR_OK, service_->OpenSession(0, CKF_SERIAL_SESSION, &session));
  EXPECT_EQ(session, 10);
}

TEST_F(TestService, CloseSession) {
  EXPECT_CALL(slot_manager_, CloseSession(0))
    .WillOnce(Return(false))
    .WillOnce(Return(true));
  EXPECT_EQ(CKR_SESSION_HANDLE_INVALID, service_->CloseSession(0));
  EXPECT_EQ(CKR_OK, service_->CloseSession(0));
}

TEST_F(TestService, CloseAllSessions) {
  EXPECT_CALL(slot_manager_, GetSlotCount())
    .WillRepeatedly(Return(2));
  EXPECT_CALL(slot_manager_, IsTokenPresent(_))
    .WillOnce(Return(false))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(slot_manager_, CloseAllSessions(1));
  EXPECT_EQ(CKR_SLOT_ID_INVALID, service_->CloseAllSessions(2));
  EXPECT_EQ(CKR_TOKEN_NOT_PRESENT, service_->CloseAllSessions(1));
  EXPECT_EQ(CKR_OK, service_->CloseAllSessions(1));
}

TEST_F(TestService, GetSessionInfo) {
  EXPECT_CALL(slot_manager_, GetSession(1, _))
    .WillOnce(Return(false))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(&session_), Return(true)));
  EXPECT_CALL(session_, GetSlot())
    .WillRepeatedly(Return(15));
  EXPECT_CALL(session_, GetState())
    .WillRepeatedly(Return(16));
  EXPECT_CALL(session_, IsReadOnly())
    .WillRepeatedly(Return(false));
  // Try bad arguments.
  uint64_t slot, state, flags, err;
  EXPECT_EQ(CKR_ARGUMENTS_BAD,
            service_->GetSessionInfo(1, NULL, &state, &flags, &err));
  EXPECT_EQ(CKR_ARGUMENTS_BAD,
            service_->GetSessionInfo(1, &slot, NULL, &flags, &err));
  EXPECT_EQ(CKR_ARGUMENTS_BAD,
            service_->GetSessionInfo(1, &slot, &state, NULL, &err));
  EXPECT_EQ(CKR_ARGUMENTS_BAD,
            service_->GetSessionInfo(1, &slot, &state, &flags, NULL));
  EXPECT_EQ(CKR_SESSION_HANDLE_INVALID,
            service_->GetSessionInfo(1, &slot, &state, &flags, &err));
  // Try normal case.
  ASSERT_EQ(CKR_OK, service_->GetSessionInfo(1, &slot, &state, &flags, &err));
  EXPECT_EQ(slot, 15);
  EXPECT_EQ(state, 16);
  EXPECT_EQ(flags, CKF_RW_SESSION|CKF_SERIAL_SESSION);
}

TEST_F(TestService, GetOperationState) {
  EXPECT_CALL(slot_manager_, GetSession(1, _))
    .WillOnce(Return(false))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(&session_), Return(true)));
  EXPECT_CALL(session_, IsOperationActive(_))
    .WillRepeatedly(Return(false));
  EXPECT_EQ(CKR_ARGUMENTS_BAD, service_->GetOperationState(1, NULL));
  vector<uint8_t> state;
  EXPECT_EQ(CKR_SESSION_HANDLE_INVALID, service_->GetOperationState(1, &state));
  EXPECT_EQ(CKR_OPERATION_NOT_INITIALIZED,
            service_->GetOperationState(1, &state));
  EXPECT_CALL(session_, IsOperationActive(_))
    .WillRepeatedly(Return(true));
  EXPECT_EQ(CKR_STATE_UNSAVEABLE,
            service_->GetOperationState(1, &state));
}

TEST_F(TestService, SetOperationState) {
  EXPECT_CALL(slot_manager_, GetSession(1, _))
    .WillOnce(Return(false))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(&session_), Return(true)));
  vector<uint8_t> state;
  EXPECT_EQ(CKR_SESSION_HANDLE_INVALID,
            service_->SetOperationState(1, state, 0, 0));
  EXPECT_EQ(CKR_SAVED_STATE_INVALID,
            service_->SetOperationState(1, state, 0, 0));
}

TEST_F(TestService, Login) {
  EXPECT_CALL(slot_manager_, GetSession(1, _))
    .WillOnce(Return(false))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(&session_), Return(true)));
  EXPECT_EQ(CKR_SESSION_HANDLE_INVALID, service_->Login(1, CKU_USER, NULL));
  string bad_pin("1234");
  string good_pin("111111");
  EXPECT_EQ(CKR_PIN_INCORRECT, service_->Login(1, CKU_SO, &good_pin));
  EXPECT_EQ(CKR_PIN_INCORRECT, service_->Login(1, CKU_USER, &bad_pin));
  EXPECT_EQ(CKR_OK, service_->Login(1, CKU_USER, &good_pin));
  EXPECT_EQ(CKR_OK, service_->Login(1, CKU_USER, NULL));
}

TEST_F(TestService, Logout) {
  EXPECT_CALL(slot_manager_, GetSession(1, _))
    .WillOnce(Return(false))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(&session_), Return(true)));
  EXPECT_EQ(CKR_SESSION_HANDLE_INVALID, service_->Logout(1));
  EXPECT_EQ(CKR_OK, service_->Logout(1));
}

TEST_F(TestService, CreateObject) {
  EXPECT_CALL(slot_manager_, GetSession(1, _))
    .WillOnce(Return(false))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(&session_), Return(true)));
  EXPECT_CALL(session_, CreateObject(_, 1, _))
    .WillOnce(Return(CKR_FUNCTION_FAILED))
    .WillRepeatedly(DoAll(SetArgumentPointee<2>(2), Return(CKR_OK)));
  EXPECT_EQ(CKR_ARGUMENTS_BAD,
            service_->CreateObject(1, good_attributes_, NULL));
  uint64_t object_handle;
  EXPECT_EQ(CKR_SESSION_HANDLE_INVALID,
            service_->CreateObject(1, good_attributes_, &object_handle));
  EXPECT_EQ(CKR_TEMPLATE_INCONSISTENT,
            service_->CreateObject(1, bad_attributes_, &object_handle));
  EXPECT_EQ(CKR_FUNCTION_FAILED,
            service_->CreateObject(1, good_attributes_, &object_handle));
  EXPECT_EQ(CKR_OK,
            service_->CreateObject(1, good_attributes_, &object_handle));
  EXPECT_EQ(object_handle, 2);
}

TEST_F(TestService, CopyObject) {
  EXPECT_CALL(slot_manager_, GetSession(1, _))
    .WillOnce(Return(false))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(&session_), Return(true)));
  EXPECT_CALL(session_, CopyObject(_, 1, 2, _))
    .WillOnce(Return(CKR_FUNCTION_FAILED))
    .WillRepeatedly(DoAll(SetArgumentPointee<3>(3), Return(CKR_OK)));
  EXPECT_EQ(CKR_ARGUMENTS_BAD,
            service_->CopyObject(1, 2, good_attributes_, NULL));
  uint64_t object_handle;
  EXPECT_EQ(CKR_SESSION_HANDLE_INVALID,
            service_->CopyObject(1, 2, good_attributes_, &object_handle));
  EXPECT_EQ(CKR_TEMPLATE_INCONSISTENT,
            service_->CopyObject(1, 2, bad_attributes_, &object_handle));
  EXPECT_EQ(CKR_FUNCTION_FAILED,
            service_->CopyObject(1, 2, good_attributes_, &object_handle));
  EXPECT_EQ(CKR_OK,
            service_->CopyObject(1, 2, good_attributes_, &object_handle));
  EXPECT_EQ(object_handle, 3);
}

TEST_F(TestService, DestroyObject) {
  EXPECT_CALL(slot_manager_, GetSession(1, _))
    .WillOnce(Return(false))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(&session_), Return(true)));
  EXPECT_CALL(session_, DestroyObject(_))
    .WillOnce(Return(CKR_FUNCTION_FAILED))
    .WillRepeatedly(Return(CKR_OK));
  EXPECT_EQ(CKR_SESSION_HANDLE_INVALID, service_->DestroyObject(1, 2));
  EXPECT_EQ(CKR_FUNCTION_FAILED, service_->DestroyObject(1, 2));
  EXPECT_EQ(CKR_OK, service_->DestroyObject(1, 2));
}

TEST_F(TestService, GetObjectSize) {
  EXPECT_CALL(slot_manager_, GetSession(1, _))
    .WillOnce(Return(false))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(&session_), Return(true)));
  EXPECT_CALL(session_, GetObject(2, _))
    .WillOnce(Return(false))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(&object_), Return(true)));
  EXPECT_CALL(object_, GetSize())
    .WillRepeatedly(Return(3));
  EXPECT_EQ(CKR_ARGUMENTS_BAD, service_->GetObjectSize(1, 2, NULL));
  uint64_t size = 0;
  EXPECT_EQ(CKR_SESSION_HANDLE_INVALID, service_->GetObjectSize(1, 2, &size));
  EXPECT_EQ(CKR_OBJECT_HANDLE_INVALID, service_->GetObjectSize(1, 2, &size));
  EXPECT_EQ(CKR_OK, service_->GetObjectSize(1, 2, &size));
  EXPECT_EQ(size, 3);
}

TEST_F(TestService, GetAttributeValue) {
  EXPECT_CALL(slot_manager_, GetSession(1, _))
    .WillOnce(Return(false))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(&session_), Return(true)));
  EXPECT_CALL(session_, GetObject(2, _))
    .WillOnce(Return(false))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(&object_), Return(true)));
  EXPECT_CALL(object_, GetAttributes(_, 1))
    .WillOnce(Return(CKR_TEMPLATE_INCONSISTENT))
    .WillOnce(Return(CKR_ATTRIBUTE_SENSITIVE))
    .WillOnce(Return(CKR_ATTRIBUTE_TYPE_INVALID))
    .WillRepeatedly(Return(CKR_OK));
  EXPECT_EQ(CKR_ARGUMENTS_BAD,
            service_->GetAttributeValue(1, 2, good_attributes_, NULL));
  vector<uint8_t> output;
  EXPECT_EQ(CKR_SESSION_HANDLE_INVALID,
            service_->GetAttributeValue(1, 2, good_attributes_, &output));
  EXPECT_EQ(CKR_OBJECT_HANDLE_INVALID,
            service_->GetAttributeValue(1, 2, good_attributes_, &output));
  EXPECT_EQ(CKR_TEMPLATE_INCONSISTENT,
            service_->GetAttributeValue(1, 2, bad_attributes_, &output));
  EXPECT_EQ(CKR_TEMPLATE_INCONSISTENT,
            service_->GetAttributeValue(1, 2, good_attributes_, &output));
  EXPECT_EQ(output.size(), 0);
  EXPECT_EQ(CKR_ATTRIBUTE_SENSITIVE,
            service_->GetAttributeValue(1, 2, good_attributes_, &output));

  // Construct a template with a valid pointer to test serialization when the
  // mock returns CKR_ATTRIBUTE_TYPE_INVALID.
  int out_value = 1234;
  CK_ATTRIBUTE invalid_type = {0, &out_value, ~0UL};
  Attributes tmp(&invalid_type, 1);
  vector<uint8_t> tmp_serialized;
  tmp.Serialize(&tmp_serialized);
  EXPECT_EQ(CKR_ATTRIBUTE_TYPE_INVALID,
            service_->GetAttributeValue(1, 2, tmp_serialized, &output));
  EXPECT_EQ(CKR_OK,
            service_->GetAttributeValue(1, 2, good_attributes_, &output));
}

TEST_F(TestService, SetAttributeValue) {
  EXPECT_CALL(slot_manager_, GetSession(1, _))
    .WillOnce(Return(false))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(&session_), Return(true)));
  EXPECT_CALL(session_, GetModifiableObject(2, _))
    .WillOnce(Return(false))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(&object_), Return(true)));
  EXPECT_CALL(object_, SetAttributes(_, 1))
    .WillOnce(Return(CKR_TEMPLATE_INCONSISTENT))
    .WillRepeatedly(Return(CKR_OK));
  vector<uint8_t> output;
  EXPECT_EQ(CKR_SESSION_HANDLE_INVALID,
            service_->SetAttributeValue(1, 2, good_attributes_));
  EXPECT_EQ(CKR_OBJECT_HANDLE_INVALID,
            service_->SetAttributeValue(1, 2, good_attributes_));
  EXPECT_EQ(CKR_TEMPLATE_INCONSISTENT,
            service_->SetAttributeValue(1, 2, bad_attributes_));
  EXPECT_EQ(CKR_TEMPLATE_INCONSISTENT,
            service_->SetAttributeValue(1, 2, good_attributes_));
  EXPECT_EQ(CKR_OK, service_->SetAttributeValue(1, 2, good_attributes_));
}

TEST_F(TestService, FindObjectsInit) {
  EXPECT_CALL(slot_manager_, GetSession(1, _))
    .WillOnce(Return(false))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(&session_), Return(true)));
  EXPECT_CALL(session_, FindObjectsInit(_, 1))
    .WillOnce(Return(CKR_FUNCTION_FAILED))
    .WillRepeatedly(Return(CKR_OK));
  EXPECT_EQ(CKR_SESSION_HANDLE_INVALID,
            service_->FindObjectsInit(1, good_attributes_));
  EXPECT_EQ(CKR_TEMPLATE_INCONSISTENT,
            service_->FindObjectsInit(1, bad_attributes_));
  EXPECT_EQ(CKR_FUNCTION_FAILED,
            service_->FindObjectsInit(1, good_attributes_));
  EXPECT_EQ(CKR_OK, service_->FindObjectsInit(1, good_attributes_));
}

TEST_F(TestService, FindObjects) {
  vector<uint64_t> objects_ret(12, 12);
  vector<int> objects_mock(12, 12);
  EXPECT_CALL(slot_manager_, GetSession(1, _))
    .WillOnce(Return(false))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(&session_), Return(true)));
  EXPECT_CALL(session_, FindObjects(2, _))
    .WillOnce(Return(CKR_FUNCTION_FAILED))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(objects_mock), Return(CKR_OK)));
  EXPECT_EQ(CKR_ARGUMENTS_BAD, service_->FindObjects(1, 2, NULL));
  vector<uint64_t> objects(1, 1);
  EXPECT_EQ(CKR_ARGUMENTS_BAD, service_->FindObjects(1, 2, &objects));
  objects.clear();
  EXPECT_EQ(CKR_SESSION_HANDLE_INVALID, service_->FindObjects(1, 2, &objects));
  EXPECT_EQ(CKR_FUNCTION_FAILED, service_->FindObjects(1, 2, &objects));
  EXPECT_EQ(CKR_OK, service_->FindObjects(1, 2, &objects));
  EXPECT_TRUE(objects == objects_ret);
}

TEST_F(TestService, FindObjectsFinal) {
  EXPECT_CALL(slot_manager_, GetSession(1, _))
    .WillOnce(Return(false))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(&session_), Return(true)));
  EXPECT_CALL(session_, FindObjectsFinal())
    .WillOnce(Return(CKR_FUNCTION_FAILED))
    .WillRepeatedly(Return(CKR_OK));
  EXPECT_EQ(CKR_SESSION_HANDLE_INVALID, service_->FindObjectsFinal(1));
  EXPECT_EQ(CKR_FUNCTION_FAILED, service_->FindObjectsFinal(1));
  EXPECT_EQ(CKR_OK, service_->FindObjectsFinal(1));
}

TEST_F(TestService, EncryptInit) {
  EXPECT_CALL(slot_manager_, GetSession(1, _))
    .WillOnce(Return(false))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(&session_), Return(true)));
  EXPECT_CALL(session_, GetObject(3, _))
    .WillOnce(Return(false))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(&object_), Return(true)));
  EXPECT_CALL(session_, OperationInit(kEncrypt, 2, _, _))
    .WillOnce(Return(CKR_FUNCTION_FAILED))
    .WillRepeatedly(Return(CKR_OK));
  vector<uint8_t> p(10, 0x10);
  EXPECT_EQ(CKR_SESSION_HANDLE_INVALID, service_->EncryptInit(1, 2, p, 3));
  EXPECT_EQ(CKR_KEY_HANDLE_INVALID, service_->EncryptInit(1, 2, p, 3));
  EXPECT_EQ(CKR_FUNCTION_FAILED, service_->EncryptInit(1, 2, p, 3));
  EXPECT_EQ(CKR_OK, service_->EncryptInit(1, 2, p, 3));
}

TEST_F(TestService, Encrypt) {
  EXPECT_CALL(slot_manager_, GetSession(1, _))
    .WillOnce(Return(false))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(&session_), Return(true)));
  EXPECT_CALL(session_, OperationSinglePart(kEncrypt, _, _, _))
    .WillOnce(Return(CKR_FUNCTION_FAILED))
    .WillRepeatedly(DoAll(SetArgumentPointee<2>(7), Return(CKR_OK)));
  vector<uint8_t> data;
  uint64_t len = 0;
  EXPECT_EQ(CKR_ARGUMENTS_BAD, service_->Encrypt(1, data, 2, NULL, &data));
  EXPECT_EQ(CKR_ARGUMENTS_BAD, service_->Encrypt(1, data, 2, &len, NULL));
  EXPECT_EQ(CKR_SESSION_HANDLE_INVALID,
            service_->Encrypt(1, data, 2, &len, &data));
  EXPECT_EQ(CKR_FUNCTION_FAILED, service_->Encrypt(1, data, 2, &len, &data));
  EXPECT_EQ(CKR_OK, service_->Encrypt(1, data, 2, &len, &data));
  EXPECT_EQ(len, 7);
}

TEST_F(TestService, EncryptUpdate) {
  EXPECT_CALL(slot_manager_, GetSession(1, _))
    .WillOnce(Return(false))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(&session_), Return(true)));
  EXPECT_CALL(session_, OperationUpdate(kEncrypt, _, _, _))
    .WillOnce(Return(CKR_FUNCTION_FAILED))
    .WillRepeatedly(DoAll(SetArgumentPointee<2>(7), Return(CKR_OK)));
  vector<uint8_t> data;
  uint64_t len = 0;
  EXPECT_EQ(CKR_ARGUMENTS_BAD,
            service_->EncryptUpdate(1, data, 2, NULL, &data));
  EXPECT_EQ(CKR_ARGUMENTS_BAD, service_->EncryptUpdate(1, data, 2, &len, NULL));
  EXPECT_EQ(CKR_SESSION_HANDLE_INVALID,
            service_->EncryptUpdate(1, data, 2, &len, &data));
  EXPECT_EQ(CKR_FUNCTION_FAILED,
            service_->EncryptUpdate(1, data, 2, &len, &data));
  EXPECT_EQ(CKR_OK, service_->EncryptUpdate(1, data, 2, &len, &data));
  EXPECT_EQ(len, 7);
}

TEST_F(TestService, EncryptFinal) {
  EXPECT_CALL(slot_manager_, GetSession(1, _))
    .WillOnce(Return(false))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(&session_), Return(true)));
  EXPECT_CALL(session_, OperationFinal(kEncrypt, _, _))
    .WillOnce(Return(CKR_FUNCTION_FAILED))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(7), Return(CKR_OK)));
  vector<uint8_t> data;
  uint64_t len = 0;
  EXPECT_EQ(CKR_ARGUMENTS_BAD, service_->EncryptFinal(1, 2, NULL, &data));
  EXPECT_EQ(CKR_ARGUMENTS_BAD, service_->EncryptFinal(1, 2, &len, NULL));
  EXPECT_EQ(CKR_SESSION_HANDLE_INVALID,
            service_->EncryptFinal(1, 2, &len, &data));
  EXPECT_EQ(CKR_FUNCTION_FAILED, service_->EncryptFinal(1, 2, &len, &data));
  EXPECT_EQ(CKR_OK, service_->EncryptFinal(1, 2, &len, &data));
  EXPECT_EQ(len, 7);
}

TEST_F(TestService, DecryptInit) {
  EXPECT_CALL(slot_manager_, GetSession(1, _))
    .WillOnce(Return(false))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(&session_), Return(true)));
  EXPECT_CALL(session_, GetObject(3, _))
    .WillOnce(Return(false))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(&object_), Return(true)));
  EXPECT_CALL(session_, OperationInit(kDecrypt, 2, _, _))
    .WillOnce(Return(CKR_FUNCTION_FAILED))
    .WillRepeatedly(Return(CKR_OK));
  vector<uint8_t> p(10, 0x10);
  EXPECT_EQ(CKR_SESSION_HANDLE_INVALID, service_->DecryptInit(1, 2, p, 3));
  EXPECT_EQ(CKR_KEY_HANDLE_INVALID, service_->DecryptInit(1, 2, p, 3));
  EXPECT_EQ(CKR_FUNCTION_FAILED, service_->DecryptInit(1, 2, p, 3));
  EXPECT_EQ(CKR_OK, service_->DecryptInit(1, 2, p, 3));
}

TEST_F(TestService, Decrypt) {
  EXPECT_CALL(slot_manager_, GetSession(1, _))
    .WillOnce(Return(false))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(&session_), Return(true)));
  EXPECT_CALL(session_, OperationSinglePart(kDecrypt, _, _, _))
    .WillOnce(Return(CKR_FUNCTION_FAILED))
    .WillRepeatedly(DoAll(SetArgumentPointee<2>(7), Return(CKR_OK)));
  vector<uint8_t> data;
  uint64_t len = 0;
  EXPECT_EQ(CKR_ARGUMENTS_BAD, service_->Decrypt(1, data, 2, NULL, &data));
  EXPECT_EQ(CKR_ARGUMENTS_BAD, service_->Decrypt(1, data, 2, &len, NULL));
  EXPECT_EQ(CKR_SESSION_HANDLE_INVALID,
            service_->Decrypt(1, data, 2, &len, &data));
  EXPECT_EQ(CKR_FUNCTION_FAILED, service_->Decrypt(1, data, 2, &len, &data));
  EXPECT_EQ(CKR_OK, service_->Decrypt(1, data, 2, &len, &data));
  EXPECT_EQ(len, 7);
}

TEST_F(TestService, DecryptUpdate) {
  EXPECT_CALL(slot_manager_, GetSession(1, _))
    .WillOnce(Return(false))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(&session_), Return(true)));
  EXPECT_CALL(session_, OperationUpdate(kDecrypt, _, _, _))
    .WillOnce(Return(CKR_FUNCTION_FAILED))
    .WillRepeatedly(DoAll(SetArgumentPointee<2>(7), Return(CKR_OK)));
  vector<uint8_t> data;
  uint64_t len = 0;
  EXPECT_EQ(CKR_ARGUMENTS_BAD,
            service_->DecryptUpdate(1, data, 2, NULL, &data));
  EXPECT_EQ(CKR_ARGUMENTS_BAD, service_->DecryptUpdate(1, data, 2, &len, NULL));
  EXPECT_EQ(CKR_SESSION_HANDLE_INVALID,
            service_->DecryptUpdate(1, data, 2, &len, &data));
  EXPECT_EQ(CKR_FUNCTION_FAILED,
            service_->DecryptUpdate(1, data, 2, &len, &data));
  EXPECT_EQ(CKR_OK, service_->DecryptUpdate(1, data, 2, &len, &data));
  EXPECT_EQ(len, 7);
}

TEST_F(TestService, DecryptFinal) {
  EXPECT_CALL(slot_manager_, GetSession(1, _))
    .WillOnce(Return(false))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(&session_), Return(true)));
  EXPECT_CALL(session_, OperationFinal(kDecrypt, _, _))
    .WillOnce(Return(CKR_FUNCTION_FAILED))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(7), Return(CKR_OK)));
  vector<uint8_t> data;
  uint64_t len = 0;
  EXPECT_EQ(CKR_ARGUMENTS_BAD, service_->DecryptFinal(1, 2, NULL, &data));
  EXPECT_EQ(CKR_ARGUMENTS_BAD, service_->DecryptFinal(1, 2, &len, NULL));
  EXPECT_EQ(CKR_SESSION_HANDLE_INVALID,
            service_->DecryptFinal(1, 2, &len, &data));
  EXPECT_EQ(CKR_FUNCTION_FAILED, service_->DecryptFinal(1, 2, &len, &data));
  EXPECT_EQ(CKR_OK, service_->DecryptFinal(1, 2, &len, &data));
  EXPECT_EQ(len, 7);
}

TEST_F(TestService, DigestInit) {
  EXPECT_CALL(slot_manager_, GetSession(1, _))
    .WillOnce(Return(false))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(&session_), Return(true)));
  EXPECT_CALL(session_, OperationInit(kDigest, 2, _, NULL))
    .WillOnce(Return(CKR_FUNCTION_FAILED))
    .WillRepeatedly(Return(CKR_OK));
  vector<uint8_t> p(10, 0x10);
  EXPECT_EQ(CKR_SESSION_HANDLE_INVALID, service_->DigestInit(1, 2, p));
  EXPECT_EQ(CKR_FUNCTION_FAILED, service_->DigestInit(1, 2, p));
  EXPECT_EQ(CKR_OK, service_->DigestInit(1, 2, p));
}

TEST_F(TestService, Digest) {
  EXPECT_CALL(slot_manager_, GetSession(1, _))
    .WillOnce(Return(false))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(&session_), Return(true)));
  EXPECT_CALL(session_, OperationSinglePart(kDigest, _, _, _))
    .WillOnce(Return(CKR_FUNCTION_FAILED))
    .WillRepeatedly(DoAll(SetArgumentPointee<2>(7), Return(CKR_OK)));
  vector<uint8_t> data;
  uint64_t len = 0;
  EXPECT_EQ(CKR_ARGUMENTS_BAD, service_->Digest(1, data, 2, NULL, &data));
  EXPECT_EQ(CKR_ARGUMENTS_BAD, service_->Digest(1, data, 2, &len, NULL));
  EXPECT_EQ(CKR_SESSION_HANDLE_INVALID,
            service_->Digest(1, data, 2, &len, &data));
  EXPECT_EQ(CKR_FUNCTION_FAILED, service_->Digest(1, data, 2, &len, &data));
  EXPECT_EQ(CKR_OK, service_->Digest(1, data, 2, &len, &data));
  EXPECT_EQ(len, 7);
}

TEST_F(TestService, DigestUpdate) {
  EXPECT_CALL(slot_manager_, GetSession(1, _))
    .WillOnce(Return(false))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(&session_), Return(true)));
  EXPECT_CALL(session_, OperationUpdate(kDigest, _, NULL, NULL))
    .WillOnce(Return(CKR_FUNCTION_FAILED))
    .WillRepeatedly(Return(CKR_OK));
  vector<uint8_t> data;
  EXPECT_EQ(CKR_SESSION_HANDLE_INVALID, service_->DigestUpdate(1, data));
  EXPECT_EQ(CKR_FUNCTION_FAILED, service_->DigestUpdate(1, data));
  EXPECT_EQ(CKR_OK, service_->DigestUpdate(1, data));
}

TEST_F(TestService, DigestFinal) {
  EXPECT_CALL(slot_manager_, GetSession(1, _))
    .WillOnce(Return(false))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(&session_), Return(true)));
  EXPECT_CALL(session_, OperationFinal(kDigest, _, _))
    .WillOnce(Return(CKR_FUNCTION_FAILED))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(7), Return(CKR_OK)));
  vector<uint8_t> data;
  uint64_t len = 0;
  EXPECT_EQ(CKR_ARGUMENTS_BAD, service_->DigestFinal(1, 2, NULL, &data));
  EXPECT_EQ(CKR_ARGUMENTS_BAD, service_->DigestFinal(1, 2, &len, NULL));
  EXPECT_EQ(CKR_SESSION_HANDLE_INVALID,
            service_->DigestFinal(1, 2, &len, &data));
  EXPECT_EQ(CKR_FUNCTION_FAILED, service_->DigestFinal(1, 2, &len, &data));
  EXPECT_EQ(CKR_OK, service_->DigestFinal(1, 2, &len, &data));
  EXPECT_EQ(len, 7);
}

TEST_F(TestService, SignInit) {
  EXPECT_CALL(slot_manager_, GetSession(1, _))
    .WillOnce(Return(false))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(&session_), Return(true)));
  EXPECT_CALL(session_, GetObject(3, _))
    .WillOnce(Return(false))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(&object_), Return(true)));
  EXPECT_CALL(session_, OperationInit(kSign, 2, _, _))
    .WillOnce(Return(CKR_FUNCTION_FAILED))
    .WillRepeatedly(Return(CKR_OK));
  vector<uint8_t> p(10, 0x10);
  EXPECT_EQ(CKR_SESSION_HANDLE_INVALID, service_->SignInit(1, 2, p, 3));
  EXPECT_EQ(CKR_KEY_HANDLE_INVALID, service_->SignInit(1, 2, p, 3));
  EXPECT_EQ(CKR_FUNCTION_FAILED, service_->SignInit(1, 2, p, 3));
  EXPECT_EQ(CKR_OK, service_->SignInit(1, 2, p, 3));
}

TEST_F(TestService, Sign) {
  EXPECT_CALL(slot_manager_, GetSession(1, _))
    .WillOnce(Return(false))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(&session_), Return(true)));
  EXPECT_CALL(session_, OperationSinglePart(kSign, _, _, _))
    .WillOnce(Return(CKR_FUNCTION_FAILED))
    .WillRepeatedly(DoAll(SetArgumentPointee<2>(7), Return(CKR_OK)));
  vector<uint8_t> data;
  uint64_t len = 0;
  EXPECT_EQ(CKR_ARGUMENTS_BAD, service_->Sign(1, data, 2, NULL, &data));
  EXPECT_EQ(CKR_ARGUMENTS_BAD, service_->Sign(1, data, 2, &len, NULL));
  EXPECT_EQ(CKR_SESSION_HANDLE_INVALID,
            service_->Sign(1, data, 2, &len, &data));
  EXPECT_EQ(CKR_FUNCTION_FAILED, service_->Sign(1, data, 2, &len, &data));
  EXPECT_EQ(CKR_OK, service_->Sign(1, data, 2, &len, &data));
  EXPECT_EQ(len, 7);
}

TEST_F(TestService, SignUpdate) {
  EXPECT_CALL(slot_manager_, GetSession(1, _))
    .WillOnce(Return(false))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(&session_), Return(true)));
  EXPECT_CALL(session_, OperationUpdate(kSign, _, NULL, NULL))
    .WillOnce(Return(CKR_FUNCTION_FAILED))
    .WillRepeatedly(Return(CKR_OK));
  vector<uint8_t> data;
  EXPECT_EQ(CKR_SESSION_HANDLE_INVALID, service_->SignUpdate(1, data));
  EXPECT_EQ(CKR_FUNCTION_FAILED, service_->SignUpdate(1, data));
  EXPECT_EQ(CKR_OK, service_->SignUpdate(1, data));
}

TEST_F(TestService, SignFinal) {
  EXPECT_CALL(slot_manager_, GetSession(1, _))
    .WillOnce(Return(false))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(&session_), Return(true)));
  EXPECT_CALL(session_, OperationFinal(kSign, _, _))
    .WillOnce(Return(CKR_FUNCTION_FAILED))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(7), Return(CKR_OK)));
  vector<uint8_t> data;
  uint64_t len = 0;
  EXPECT_EQ(CKR_ARGUMENTS_BAD, service_->SignFinal(1, 2, NULL, &data));
  EXPECT_EQ(CKR_ARGUMENTS_BAD, service_->SignFinal(1, 2, &len, NULL));
  EXPECT_EQ(CKR_SESSION_HANDLE_INVALID,
            service_->SignFinal(1, 2, &len, &data));
  EXPECT_EQ(CKR_FUNCTION_FAILED, service_->SignFinal(1, 2, &len, &data));
  EXPECT_EQ(CKR_OK, service_->SignFinal(1, 2, &len, &data));
  EXPECT_EQ(len, 7);
}

TEST_F(TestService, VerifyInit) {
  EXPECT_CALL(slot_manager_, GetSession(1, _))
    .WillOnce(Return(false))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(&session_), Return(true)));
  EXPECT_CALL(session_, GetObject(3, _))
    .WillOnce(Return(false))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(&object_), Return(true)));
  EXPECT_CALL(session_, OperationInit(kVerify, 2, _, _))
    .WillOnce(Return(CKR_FUNCTION_FAILED))
    .WillRepeatedly(Return(CKR_OK));
  vector<uint8_t> p(10, 0x10);
  EXPECT_EQ(CKR_SESSION_HANDLE_INVALID, service_->VerifyInit(1, 2, p, 3));
  EXPECT_EQ(CKR_KEY_HANDLE_INVALID, service_->VerifyInit(1, 2, p, 3));
  EXPECT_EQ(CKR_FUNCTION_FAILED, service_->VerifyInit(1, 2, p, 3));
  EXPECT_EQ(CKR_OK, service_->VerifyInit(1, 2, p, 3));
}

TEST_F(TestService, Verify) {
  EXPECT_CALL(slot_manager_, GetSession(1, _))
    .WillOnce(Return(false))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(&session_), Return(true)));
  EXPECT_CALL(session_, OperationUpdate(kVerify, _, NULL, NULL))
    .WillOnce(Return(CKR_FUNCTION_FAILED))
    .WillRepeatedly(Return(CKR_OK));
  EXPECT_CALL(session_, VerifyFinal(_))
    .WillOnce(Return(CKR_FUNCTION_FAILED))
    .WillRepeatedly(Return(CKR_OK));
  vector<uint8_t> data;
  EXPECT_EQ(CKR_SESSION_HANDLE_INVALID, service_->Verify(1, data, data));
  EXPECT_EQ(CKR_FUNCTION_FAILED, service_->Verify(1, data, data));
  EXPECT_EQ(CKR_FUNCTION_FAILED, service_->Verify(1, data, data));
  EXPECT_EQ(CKR_OK, service_->Verify(1, data, data));
}

TEST_F(TestService, VerifyUpdate) {
  EXPECT_CALL(slot_manager_, GetSession(1, _))
    .WillOnce(Return(false))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(&session_), Return(true)));
  EXPECT_CALL(session_, OperationUpdate(kVerify, _, NULL, NULL))
    .WillOnce(Return(CKR_FUNCTION_FAILED))
    .WillRepeatedly(Return(CKR_OK));
  vector<uint8_t> data;
  EXPECT_EQ(CKR_SESSION_HANDLE_INVALID, service_->VerifyUpdate(1, data));
  EXPECT_EQ(CKR_FUNCTION_FAILED, service_->VerifyUpdate(1, data));
  EXPECT_EQ(CKR_OK, service_->VerifyUpdate(1, data));
}

TEST_F(TestService, VerifyFinal) {
  EXPECT_CALL(slot_manager_, GetSession(1, _))
    .WillOnce(Return(false))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(&session_), Return(true)));
  EXPECT_CALL(session_, VerifyFinal(_))
    .WillOnce(Return(CKR_FUNCTION_FAILED))
    .WillRepeatedly(Return(CKR_OK));
  vector<uint8_t> data;
  EXPECT_EQ(CKR_SESSION_HANDLE_INVALID, service_->VerifyFinal(1, data));
  EXPECT_EQ(CKR_FUNCTION_FAILED, service_->VerifyFinal(1, data));
  EXPECT_EQ(CKR_OK, service_->VerifyFinal(1, data));
}

TEST_F(TestService, GenerateKey) {
  EXPECT_CALL(slot_manager_, GetSession(1, _))
    .WillOnce(Return(false))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(&session_), Return(true)));
  EXPECT_CALL(session_, GenerateKey(2, _, _, 1, _))
    .WillOnce(Return(CKR_FUNCTION_FAILED))
    .WillRepeatedly(DoAll(SetArgumentPointee<4>(3), Return(CKR_OK)));
  vector<uint8_t> param;
  uint64_t handle;
  EXPECT_EQ(CKR_SESSION_HANDLE_INVALID,
            service_->GenerateKey(1, 2, param, good_attributes_, &handle));
  EXPECT_EQ(CKR_TEMPLATE_INCONSISTENT,
            service_->GenerateKey(1, 2, param, bad_attributes_, &handle));
  EXPECT_EQ(CKR_FUNCTION_FAILED,
            service_->GenerateKey(1, 2, param, good_attributes_, &handle));
  EXPECT_EQ(CKR_OK,
            service_->GenerateKey(1, 2, param, good_attributes_, &handle));
  EXPECT_EQ(handle, 3);
}

TEST_F(TestService, GenerateKeyPair) {
  EXPECT_CALL(slot_manager_, GetSession(1, _))
    .WillOnce(Return(false))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(&session_), Return(true)));
  EXPECT_CALL(session_, GenerateKeyPair(2, _, _, 1, _, 1, _, _))
    .WillOnce(Return(CKR_FUNCTION_FAILED))
    .WillRepeatedly(DoAll(SetArgumentPointee<6>(3),
                          SetArgumentPointee<7>(4),
                          Return(CKR_OK)));
  vector<uint8_t> param;
  uint64_t handle[2];
  EXPECT_EQ(CKR_SESSION_HANDLE_INVALID,
            service_->GenerateKeyPair(1, 2, param,
                                      good_attributes_, good_attributes2_,
                                      &handle[0], &handle[1]));
  EXPECT_EQ(CKR_TEMPLATE_INCONSISTENT,
            service_->GenerateKeyPair(1, 2, param,
                                      bad_attributes_, good_attributes2_,
                                      &handle[0], &handle[1]));
  EXPECT_EQ(CKR_TEMPLATE_INCONSISTENT,
            service_->GenerateKeyPair(1, 2, param,
                                      good_attributes_, bad_attributes_,
                                      &handle[0], &handle[1]));
  EXPECT_EQ(CKR_FUNCTION_FAILED, service_->GenerateKeyPair(1, 2, param,
                                                           good_attributes_,
                                                           good_attributes2_,
                                                           &handle[0],
                                                           &handle[1]));
  EXPECT_EQ(CKR_OK, service_->GenerateKeyPair(1, 2, param,
                                              good_attributes_,
                                              good_attributes2_,
                                              &handle[0],
                                              &handle[1]));
  EXPECT_EQ(handle[0], 3);
  EXPECT_EQ(handle[1], 4);
}

TEST_F(TestService, SeedRandom) {
  vector<uint8_t> seed(3, 'A');
  string seed_str("AAA");
  EXPECT_CALL(slot_manager_, GetSession(1, _))
    .WillOnce(Return(false))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(&session_), Return(true)));
  EXPECT_CALL(session_, SeedRandom(seed_str)).WillRepeatedly(Return(CKR_OK));
  EXPECT_EQ(CKR_SESSION_HANDLE_INVALID, service_->SeedRandom(1, seed));
  EXPECT_EQ(CKR_OK, service_->SeedRandom(1, seed));
}

TEST_F(TestService, GenerateRandom) {
  vector<uint8_t> random_data(3, 'B');
  string random_data_str("BBB");
  EXPECT_CALL(slot_manager_, GetSession(1, _))
    .WillOnce(Return(false))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(&session_), Return(true)));
  EXPECT_CALL(session_, GenerateRandom(8, _))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(random_data_str),
                          Return(CKR_OK)));
  vector<uint8_t> output;
  EXPECT_EQ(CKR_SESSION_HANDLE_INVALID,
            service_->GenerateRandom(1, 8, &output));
  EXPECT_EQ(CKR_OK, service_->GenerateRandom(1, 8, &output));
  EXPECT_TRUE(output == random_data);
}

}  // namespace chaps

int main(int argc, char** argv) {
  ::testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}
