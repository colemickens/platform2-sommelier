// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include <gflags/gflags.h>
#include <gtest/gtest.h>

#include "chaps/attributes.h"
#include "chaps/chaps_interface.h"
#include "chaps/chaps_proxy.h"
#include "chaps/chaps_service_redirect.h"
#include "chaps/chaps_utility.h"

using std::string;
using std::vector;

DEFINE_bool(use_dbus, false, "");

namespace chaps {

static chaps::ChapsInterface* CreateChapsInstance() {
  if (FLAGS_use_dbus) {
    scoped_ptr<chaps::ChapsProxyImpl> proxy(new chaps::ChapsProxyImpl());
    if (proxy->Init())
      return proxy.release();
  } else {
    scoped_ptr<chaps::ChapsServiceRedirect> service(
        new chaps::ChapsServiceRedirect("libopencryptoki.so"));
    if (service->Init())
      return service.release();
  }
  return NULL;
}

static bool SerializeAttributes(CK_ATTRIBUTE_PTR attributes,
                                CK_ULONG num_attributes,
                                vector<uint8_t>* serialized) {
  Attributes tmp(attributes, num_attributes);
  return tmp.Serialize(serialized);
}

static bool ParseAndFillAttributes(const vector<uint8_t>& serialized,
                                   CK_ATTRIBUTE_PTR attributes,
                                   CK_ULONG num_attributes) {
  Attributes tmp(attributes, num_attributes);
  return tmp.ParseAndFill(serialized);
}

static vector<uint8_t> SubVector(const vector<uint8_t>& v,
                                 int offset,
                                 int size) {
  const uint8_t* front = &v.front() + offset;
  return vector<uint8_t>(front, front + size);
}

// Default test fixture for PKCS #11 calls.
class TestP11 : public ::testing::Test {
protected:
  virtual void SetUp() {
    // The current user's token will be used so the token will already be
    // initialized and changes to token objects will persist.  The user pin can
    // be assumed to be "111111" and the so pin can be assumed to be "000000".
    // This approach will be used as long as we redirect to openCryptoki.
    chaps_.reset(CreateChapsInstance());
    ASSERT_TRUE(chaps_ != NULL);
    so_pin_ = "000000";
    user_pin_ = "111111";
  }
  scoped_ptr<chaps::ChapsInterface> chaps_;
  string so_pin_;
  string user_pin_;
};

// Test fixture for testing with a valid open session.
class TestP11PublicSession : public TestP11 {
 protected:
  virtual void SetUp() {
    TestP11::SetUp();
    ASSERT_EQ(CKR_OK, chaps_->OpenSession(0, CKF_SERIAL_SESSION|CKF_RW_SESSION,
                                          &session_id_));
    uint32_t result = chaps_->Logout(session_id_);
    ASSERT_TRUE(result == CKR_OK || result == CKR_USER_NOT_LOGGED_IN);
  }
  virtual void TearDown() {
    uint32_t result = chaps_->Logout(session_id_);
    ASSERT_TRUE(result == CKR_OK || result == CKR_USER_NOT_LOGGED_IN);
    EXPECT_EQ(CKR_OK, chaps_->CloseSession(session_id_));
    TestP11::TearDown();
  }
  uint32_t session_id_;
};

class TestP11UserSession : public TestP11PublicSession {
 protected:
  virtual void SetUp() {
    TestP11PublicSession::SetUp();
    uint32_t result = chaps_->Login(session_id_, CKU_USER, &user_pin_);
    ASSERT_TRUE(result == CKR_OK || result == CKR_USER_ALREADY_LOGGED_IN);
  }
  virtual void TearDown() {
    TestP11PublicSession::TearDown();
  }
};

class TestP11SOSession : public TestP11PublicSession {
 protected:
  virtual void SetUp() {
    TestP11PublicSession::SetUp();
    uint32_t result = chaps_->Login(session_id_, CKU_SO, &so_pin_);
    ASSERT_TRUE(result == CKR_OK || result == CKR_USER_ALREADY_LOGGED_IN);
  }
  virtual void TearDown() {
    TestP11PublicSession::TearDown();
  }
};

class TestP11Object : public TestP11PublicSession {
 protected:
  virtual void SetUp() {
    TestP11PublicSession::SetUp();
    CK_OBJECT_CLASS class_value = CKO_DATA;
    CK_UTF8CHAR label[] = "A data object";
    CK_UTF8CHAR application[] = "An application";
    CK_BYTE data[] = "Sample data";
    CK_BBOOL false_value = CK_FALSE;
    CK_ATTRIBUTE attributes[] = {
      {CKA_CLASS, &class_value, sizeof(class_value)},
      {CKA_TOKEN, &false_value, sizeof(false_value)},
      {CKA_LABEL, label, sizeof(label)-1},
      {CKA_APPLICATION, application, sizeof(application)-1},
      {CKA_VALUE, data, sizeof(data)}
    };
    vector<uint8_t> serialized;
    ASSERT_TRUE(SerializeAttributes(attributes, 5, &serialized));
    ASSERT_EQ(CKR_OK, chaps_->CreateObject(session_id_,
                                           serialized,
                                           &object_handle_));
  }
  virtual void TearDown() {
    ASSERT_EQ(CKR_OK, chaps_->DestroyObject(session_id_, object_handle_));
    TestP11PublicSession::TearDown();
  }
  uint32_t object_handle_;
};

TEST_F(TestP11, SlotList) {
  vector<uint32_t> slot_list;
  uint32_t result = chaps_->GetSlotList(false, &slot_list);
  EXPECT_EQ(CKR_OK, result);
  EXPECT_LT(0, slot_list.size());
  printf("Slots: ");
  for (size_t i = 0; i < slot_list.size(); ++i) {
    printf("%d ", slot_list[i]);
  }
  printf("\n");
  result = chaps_->GetSlotList(false, NULL);
  EXPECT_EQ(CKR_ARGUMENTS_BAD, result);
}

TEST_F(TestP11, SlotInfo) {
  string description;
  string manufacturer;
  uint32_t flags;
  uint8_t version[4];
  uint32_t result = chaps_->GetSlotInfo(0, &description,
                                        &manufacturer, &flags,
                                        &version[0], &version[1],
                                        &version[2], &version[3]);
  EXPECT_EQ(CKR_OK, result);
  printf("Slot Info: %s - %s\n", manufacturer.c_str(), description.c_str());
  result = chaps_->GetSlotInfo(0, NULL, &manufacturer, &flags,
                               &version[0], &version[1],
                               &version[2], &version[3]);
  EXPECT_EQ(CKR_ARGUMENTS_BAD, result);
  result = chaps_->GetSlotInfo(17, &description, &manufacturer, &flags,
                               &version[0], &version[1],
                               &version[2], &version[3]);
  EXPECT_NE(CKR_OK, result);
}

TEST_F(TestP11, TokenInfo) {
  string label;
  string manufacturer;
  string model;
  string serial_number;
  uint32_t flags;
  uint8_t version[4];
  uint32_t not_used[10];
  uint32_t result = chaps_->GetTokenInfo(0, &label, &manufacturer,
                                         &model, &serial_number, &flags,
                                         &not_used[0], &not_used[1],
                                         &not_used[2], &not_used[3],
                                         &not_used[4], &not_used[5],
                                         &not_used[6], &not_used[7],
                                         &not_used[8], &not_used[9],
                                         &version[0], &version[1],
                                         &version[2], &version[3]);
  EXPECT_EQ(CKR_OK, result);
  printf("Token Info: %s - %s - %s - %s\n",
         manufacturer.c_str(),
         model.c_str(),
         label.c_str(),
         serial_number.c_str());
  result = chaps_->GetTokenInfo(0, NULL, &manufacturer,
                                &model, &serial_number, &flags,
                                &not_used[0], &not_used[1],
                                &not_used[2], &not_used[3],
                                &not_used[4], &not_used[5],
                                &not_used[6], &not_used[7],
                                &not_used[8], &not_used[9],
                                &version[0], &version[1],
                                &version[2], &version[3]);
  EXPECT_EQ(CKR_ARGUMENTS_BAD, result);
  result = chaps_->GetTokenInfo(17, &label, &manufacturer,
                                &model, &serial_number, &flags,
                                &not_used[0], &not_used[1],
                                &not_used[2], &not_used[3],
                                &not_used[4], &not_used[5],
                                &not_used[6], &not_used[7],
                                &not_used[8], &not_used[9],
                                &version[0], &version[1],
                                &version[2], &version[3]);
  EXPECT_NE(CKR_OK, result);
}

TEST_F(TestP11, MechList) {
  vector<uint32_t> mech_list;
  uint32_t result = chaps_->GetMechanismList(0, &mech_list);
  EXPECT_EQ(CKR_OK, result);
  EXPECT_LT(0, mech_list.size());
  printf("Mech List [0]: %d\n", mech_list[0]);
  result = chaps_->GetMechanismList(0, NULL);
  EXPECT_EQ(CKR_ARGUMENTS_BAD, result);
  result = chaps_->GetMechanismList(17, &mech_list);
  EXPECT_NE(CKR_OK, result);
}

TEST_F(TestP11, MechInfo) {
  uint32_t flags;
  uint32_t min_key_size;
  uint32_t max_key_size;
  uint32_t result = chaps_->GetMechanismInfo(0, CKM_RSA_PKCS, &min_key_size,
                                         &max_key_size, &flags);
  EXPECT_EQ(CKR_OK, result);
  printf("RSA Key Sizes: %d - %d\n", min_key_size, max_key_size);
  result = chaps_->GetMechanismInfo(0,
                                    0xFFFF,
                                    &min_key_size, &max_key_size,
                                    &flags);
  EXPECT_EQ(CKR_MECHANISM_INVALID, result);
  result = chaps_->GetMechanismInfo(17,
                                    CKM_RSA_PKCS,
                                    &min_key_size,
                                    &max_key_size,
                                    &flags);
  EXPECT_NE(CKR_OK, result);
  result = chaps_->GetMechanismInfo(0,
                                    CKM_RSA_PKCS,
                                    NULL,
                                    &max_key_size,
                                    &flags);
  EXPECT_EQ(CKR_ARGUMENTS_BAD, result);
  result = chaps_->GetMechanismInfo(0,
                                    CKM_RSA_PKCS,
                                    &min_key_size,
                                    NULL,
                                    &flags);
  EXPECT_EQ(CKR_ARGUMENTS_BAD, result);
  result = chaps_->GetMechanismInfo(0,
                                    CKM_RSA_PKCS,
                                    &min_key_size,
                                    &max_key_size,
                                    NULL);
  EXPECT_EQ(CKR_ARGUMENTS_BAD, result);
}

// TODO(dkrahn): crosbug.com/22297
// These PIN-related tests can mess up a live token.  Leave them until we're
// no longer using openCryptoki.
#ifdef CHAPS_TOKEN_INIT_TESTS
TEST_F(TestP11, InitToken) {
  string label = "test";
  ASSERT_EQ(CKR_OK, chaps_->CloseAllSessions(0));
  EXPECT_NE(CKR_OK, chaps_->InitToken(0, NULL, label));
  ASSERT_EQ(CKR_OK, chaps_->InitToken(0, &so_pin_, label);
  // Put the token back in a usable state. This is required if we're testing on
  // a live token.
  uint32_t session = 0;
  ASSERT_EQ(CKR_OK, chaps_->OpenSession(0, CKF_SERIAL_SESSION|CKF_RW_SESSION,
                                        &session));
  ASSERT_EQ(CKR_OK, chaps_->Login(session, CKU_SO, &so_pin_));
  ASSERT_EQ(CKR_OK, chaps_->InitPIN(session, &user_pin_));
  ASSERT_EQ(CKR_OK, chaps_->Logout(session));
  ASSERT_EQ(CKR_OK, chaps_->CloseSession(session));
}

TEST_F(TestP11SOSession, InitPIN) {
  EXPECT_NE(CKR_OK, chaps_->InitPIN(session_id_, NULL));
  EXPECT_EQ(CKR_OK, chaps_->InitPIN(session_id_, &user_pin_));
}

TEST_F(TestP11UserSession, SetPIN) {
  string bad_pin = "123456";
  string other_pin = "222222";
  EXPECT_NE(CKR_OK, chaps_->SetPIN(session_id_, &bad_pin, &other_pin));
  EXPECT_NE(CKR_OK, chaps_->SetPIN(session_id_, NULL, NULL));
  ASSERT_EQ(CKR_OK, chaps_->SetPIN(session_id_, &user_pin_, &other_pin));
  // Put the PIN back to what it was. This is required if we're testing on a
  // live token.
  ASSERT_EQ(CKR_OK, chaps_->SetPIN(session_id_, &other_pin, &user_pin_));
}
#endif

TEST_F(TestP11, OpenCloseSession) {
  uint32_t session = 0;
  // Test successful RO and RW sessions.
  EXPECT_EQ(CKR_OK, chaps_->OpenSession(0, CKF_SERIAL_SESSION, &session));
  EXPECT_EQ(CKR_OK, chaps_->CloseSession(session));
  EXPECT_EQ(CKR_OK, chaps_->OpenSession(0, CKF_SERIAL_SESSION|CKF_RW_SESSION,
                                        &session));
  EXPECT_EQ(CKR_OK, chaps_->CloseSession(session));
  // Test double close.
  EXPECT_EQ(CKR_SESSION_HANDLE_INVALID, chaps_->CloseSession(session));
  // Test error cases.
  EXPECT_EQ(CKR_ARGUMENTS_BAD,
            chaps_->OpenSession(0, CKF_SERIAL_SESSION, NULL));
  EXPECT_EQ(CKR_SESSION_PARALLEL_NOT_SUPPORTED,
            chaps_->OpenSession(0, 0, &session));
  // Test CloseAllSessions.
  EXPECT_EQ(CKR_OK, chaps_->OpenSession(0, CKF_SERIAL_SESSION, &session));
  EXPECT_EQ(CKR_OK, chaps_->CloseAllSessions(0));
  EXPECT_EQ(CKR_SESSION_HANDLE_INVALID, chaps_->CloseSession(session));
}

TEST_F(TestP11PublicSession, GetSessionInfo) {
  uint32_t slot_id, state, flags, device_error;
  EXPECT_EQ(CKR_OK, chaps_->GetSessionInfo(session_id_, &slot_id, &state,
                                           &flags, &device_error));
  EXPECT_EQ(0, slot_id);
  EXPECT_EQ(CKS_RW_PUBLIC_SESSION, state);
  EXPECT_EQ(CKF_SERIAL_SESSION|CKF_RW_SESSION, flags);
  uint32_t readonly_session_id;
  ASSERT_EQ(CKR_OK, chaps_->OpenSession(0, CKF_SERIAL_SESSION,
                                        &readonly_session_id));
  EXPECT_EQ(CKR_OK, chaps_->GetSessionInfo(readonly_session_id, &slot_id,
                                           &state, &flags, &device_error));
  EXPECT_EQ(CKR_OK, chaps_->CloseSession(readonly_session_id));
  EXPECT_EQ(0, slot_id);
  EXPECT_EQ(CKS_RO_PUBLIC_SESSION, state);
  EXPECT_EQ(CKF_SERIAL_SESSION, flags);
  EXPECT_EQ(CKR_SESSION_HANDLE_INVALID,
            chaps_->GetSessionInfo(17, &slot_id, &state, &flags,
                                   &device_error));
  EXPECT_EQ(CKR_ARGUMENTS_BAD, chaps_->GetSessionInfo(session_id_,
                                                      NULL,
                                                      &state,
                                                      &flags,
                                                      &device_error));
  EXPECT_EQ(CKR_ARGUMENTS_BAD, chaps_->GetSessionInfo(session_id_,
                                                      &slot_id,
                                                      NULL,
                                                      &flags,
                                                      &device_error));
  EXPECT_EQ(CKR_ARGUMENTS_BAD, chaps_->GetSessionInfo(session_id_,
                                                      &slot_id,
                                                      &state,
                                                      NULL,
                                                      &device_error));
  EXPECT_EQ(CKR_ARGUMENTS_BAD, chaps_->GetSessionInfo(session_id_,
                                                      &slot_id,
                                                      &state,
                                                      &flags,
                                                      NULL));
}

TEST_F(TestP11PublicSession, GetOperationState) {
  vector<uint8_t> state;
  EXPECT_EQ(CKR_SESSION_HANDLE_INVALID, chaps_->GetOperationState(17, &state));
  EXPECT_EQ(CKR_ARGUMENTS_BAD, chaps_->GetOperationState(session_id_, NULL));
}

TEST_F(TestP11PublicSession, SetOperationState) {
  vector<uint8_t> state(10, 0);
  EXPECT_EQ(CKR_SESSION_HANDLE_INVALID,
            chaps_->SetOperationState(17, state, 0, 0));
}

TEST_F(TestP11PublicSession, Login) {
  EXPECT_EQ(CKR_SESSION_HANDLE_INVALID,
            chaps_->Login(17, CKU_USER, &user_pin_));
  EXPECT_EQ(CKR_USER_TYPE_INVALID, chaps_->Login(session_id_, 17, &user_pin_));
  EXPECT_NE(CKR_OK, chaps_->Login(session_id_, CKU_USER, NULL));
  EXPECT_EQ(CKR_OK, chaps_->Login(session_id_, CKU_USER, &user_pin_));
}

TEST_F(TestP11PublicSession, Logout) {
  EXPECT_EQ(CKR_USER_NOT_LOGGED_IN, chaps_->Logout(session_id_));
  EXPECT_EQ(CKR_SESSION_HANDLE_INVALID, chaps_->Logout(17));
  ASSERT_EQ(CKR_OK, chaps_->Login(session_id_, CKU_USER, &user_pin_));
  EXPECT_EQ(CKR_OK, chaps_->Logout(session_id_));
}

TEST_F(TestP11PublicSession, CreateObject) {
  CK_OBJECT_CLASS class_value = CKO_DATA;
  CK_UTF8CHAR label[] = "A data object";
  CK_UTF8CHAR application[] = "An application";
  CK_BYTE data[] = "Sample data";
  CK_BYTE data2[] = "Sample data 2";
  CK_BBOOL false_value = CK_FALSE;
  CK_ATTRIBUTE attributes[] = {
    {CKA_CLASS, &class_value, sizeof(class_value)},
    {CKA_TOKEN, &false_value, sizeof(false_value)},
    {CKA_LABEL, label, sizeof(label)-1},
    {CKA_APPLICATION, application, sizeof(application)-1},
    {CKA_VALUE, data, sizeof(data)}
  };
  CK_ATTRIBUTE attributes2[] = {
    {CKA_VALUE, data2, sizeof(data2)}
  };
  vector<uint8_t> attribute_serial;
  ASSERT_TRUE(SerializeAttributes(attributes, 5, &attribute_serial));
  uint32_t handle = 0;
  EXPECT_EQ(CKR_ARGUMENTS_BAD,
            chaps_->CreateObject(session_id_, attribute_serial, NULL));
  EXPECT_EQ(CKR_OK,
            chaps_->CreateObject(session_id_, attribute_serial, &handle));
  vector<uint8_t> attribute_serial2;
  ASSERT_TRUE(SerializeAttributes(attributes2, 1, &attribute_serial2));
  uint32_t handle2 = 0;
  EXPECT_EQ(CKR_OK, chaps_->CopyObject(session_id_,
                                       handle,
                                       attribute_serial2,
                                       &handle2));
  EXPECT_EQ(CKR_OK, chaps_->DestroyObject(session_id_, handle));
  EXPECT_EQ(CKR_OK, chaps_->DestroyObject(session_id_, handle2));
  EXPECT_EQ(CKR_SESSION_HANDLE_INVALID,
            chaps_->CreateObject(17, attribute_serial, &handle));
  EXPECT_EQ(CKR_TEMPLATE_INCOMPLETE,
            chaps_->CreateObject(session_id_, attribute_serial2, &handle));
}

TEST_F(TestP11Object, GetObjectSize) {
  uint32_t size;
  EXPECT_EQ(CKR_OK, chaps_->GetObjectSize(session_id_, object_handle_, &size));
  EXPECT_EQ(CKR_ARGUMENTS_BAD,
      chaps_->GetObjectSize(session_id_, object_handle_, NULL));
  EXPECT_EQ(CKR_SESSION_HANDLE_INVALID,
      chaps_->GetObjectSize(17, object_handle_, &size));
  EXPECT_EQ(CKR_OBJECT_HANDLE_INVALID,
      chaps_->GetObjectSize(session_id_, 17, &size));
}

TEST_F(TestP11Object, GetAttributeValue) {
  CK_BYTE buffer[100];
  CK_ATTRIBUTE query[1] = {{CKA_VALUE, buffer, sizeof(buffer)}};
  vector<uint8_t> serial_query;
  ASSERT_TRUE(SerializeAttributes(query, 1, &serial_query));
  vector<uint8_t> response;
  EXPECT_EQ(CKR_OK, chaps_->GetAttributeValue(session_id_,
                                              object_handle_,
                                              serial_query,
                                              &response));
  EXPECT_TRUE(ParseAndFillAttributes(response, query, 1));
  CK_BYTE data[] = "Sample data";
  EXPECT_EQ(sizeof(data), query[0].ulValueLen);
  EXPECT_EQ(0, memcmp(data, query[0].pValue, query[0].ulValueLen));
  EXPECT_EQ(CKR_ARGUMENTS_BAD, chaps_->GetAttributeValue(session_id_,
                                                         object_handle_,
                                                         serial_query,
                                                         NULL));
  EXPECT_EQ(CKR_SESSION_HANDLE_INVALID,
      chaps_->GetAttributeValue(17, object_handle_, serial_query, &response));
  EXPECT_EQ(CKR_OBJECT_HANDLE_INVALID,
      chaps_->GetAttributeValue(session_id_, 17, serial_query, &response));
}

TEST_F(TestP11Object, SetAttributeValue) {
  CK_BYTE buffer[100];
  memset(buffer, 0xAA, sizeof(buffer));
  CK_ATTRIBUTE attributes[1] = {{CKA_VALUE, buffer, sizeof(buffer)}};
  vector<uint8_t> serial;
  ASSERT_TRUE(SerializeAttributes(attributes, 1, &serial));
  EXPECT_EQ(CKR_OK, chaps_->SetAttributeValue(session_id_,
                                              object_handle_,
                                              serial));
  CK_BYTE buffer2[100];
  memset(buffer2, 0xBB, sizeof(buffer2));
  attributes[0].pValue = buffer2;
  ASSERT_TRUE(SerializeAttributes(attributes, 1, &serial));
  vector<uint8_t> response;
  EXPECT_EQ(CKR_OK, chaps_->GetAttributeValue(session_id_,
                                              object_handle_,
                                              serial,
                                              &response));
  EXPECT_TRUE(ParseAndFillAttributes(response, attributes, 1));
  EXPECT_EQ(0, memcmp(buffer, buffer2, 100));
  EXPECT_EQ(CKR_SESSION_HANDLE_INVALID,
      chaps_->SetAttributeValue(17, object_handle_, serial));
  EXPECT_EQ(CKR_OBJECT_HANDLE_INVALID,
      chaps_->SetAttributeValue(session_id_, 17, serial));
}

TEST_F(TestP11Object, FindObjects) {
  vector<uint32_t> objects;
  vector<uint8_t> empty;
  EXPECT_EQ(CKR_OK, chaps_->FindObjectsInit(session_id_, empty));
  EXPECT_EQ(CKR_OK, chaps_->FindObjects(session_id_, 10, &objects));
  EXPECT_EQ(CKR_OK, chaps_->FindObjectsFinal(session_id_));
  EXPECT_GT(objects.size(), 0);
  EXPECT_LT(objects.size(), 11);

  CK_OBJECT_CLASS class_value = CKO_DATA;
  CK_BBOOL false_value = CK_FALSE;
  CK_ATTRIBUTE attributes[2] = {
    {CKA_CLASS, &class_value, sizeof(class_value)},
    {CKA_TOKEN, &false_value, sizeof(false_value)}
  };
  vector<uint8_t> serial;
  ASSERT_TRUE(SerializeAttributes(attributes, 2, &serial));
  EXPECT_EQ(CKR_TEMPLATE_INCONSISTENT,
            chaps_->FindObjectsInit(session_id_,
                                    vector<uint8_t>(20, 0)));
  EXPECT_EQ(CKR_OK, chaps_->FindObjectsInit(session_id_, serial));
  EXPECT_EQ(CKR_ARGUMENTS_BAD, chaps_->FindObjects(session_id_, 10, NULL));
  EXPECT_EQ(CKR_ARGUMENTS_BAD, chaps_->FindObjects(session_id_, 10, &objects));
  objects.clear();
  EXPECT_EQ(CKR_OK, chaps_->FindObjects(session_id_, 10, &objects));
  EXPECT_EQ(CKR_OK, chaps_->FindObjectsFinal(session_id_));
  EXPECT_EQ(objects.size(), 1);
  // Operation state management tests.
  objects.clear();
  EXPECT_EQ(CKR_OPERATION_NOT_INITIALIZED,
            chaps_->FindObjects(session_id_, 10, &objects));
  EXPECT_EQ(CKR_OPERATION_NOT_INITIALIZED,
            chaps_->FindObjectsFinal(session_id_));
  EXPECT_EQ(CKR_SESSION_HANDLE_INVALID, chaps_->FindObjectsInit(17, empty));
  EXPECT_EQ(CKR_SESSION_HANDLE_INVALID, chaps_->FindObjects(17, 10, &objects));
  EXPECT_EQ(CKR_SESSION_HANDLE_INVALID, chaps_->FindObjectsFinal(17));
}

TEST_F(TestP11PublicSession, Encrypt) {
  // Create a session key.
  CK_OBJECT_CLASS key_class = CKO_SECRET_KEY;
  CK_KEY_TYPE key_type = CKK_AES;
  CK_BYTE key_value[32] = {0};
  CK_BBOOL false_value = CK_FALSE;
  CK_BBOOL true_value = CK_TRUE;
  CK_ATTRIBUTE key_desc[] = {
    {CKA_CLASS, &key_class, sizeof(key_class)},
    {CKA_KEY_TYPE, &key_type, sizeof(key_type)},
    {CKA_TOKEN, &false_value, sizeof(false_value)},
    {CKA_ENCRYPT, &true_value, sizeof(true_value)},
    {CKA_DECRYPT, &true_value, sizeof(true_value)},
    {CKA_VALUE, key_value, sizeof(key_value)}
  };
  vector<uint8_t> key;
  ASSERT_TRUE(SerializeAttributes(key_desc, 6, &key));
  uint32_t key_handle = 0;
  ASSERT_EQ(CKR_OK, chaps_->CreateObject(session_id_, key, &key_handle));
  // Test encrypt.
  vector<uint8_t> parameter;
  EXPECT_EQ(CKR_OK, chaps_->EncryptInit(session_id_,
                                        CKM_AES_ECB,
                                        parameter,
                                        key_handle));
  vector<uint8_t> data(48, 2), encrypted;
  uint32_t not_used = 0;
  const uint32_t max_out = 100;
  EXPECT_EQ(CKR_OK, chaps_->Encrypt(session_id_,
                                    data,
                                    max_out,
                                    &not_used,
                                    &encrypted));
  EXPECT_EQ(CKR_OK, chaps_->EncryptInit(session_id_,
                                        CKM_AES_ECB,
                                        parameter,
                                        key_handle));
  vector<uint8_t> encrypted2, tmp;
  EXPECT_EQ(CKR_OK, chaps_->EncryptUpdate(session_id_,
                                          SubVector(data, 0, 3),
                                          max_out,
                                          &not_used,
                                          &tmp));
  encrypted2.insert(encrypted2.end(), tmp.begin(), tmp.end());
  tmp.clear();
  EXPECT_EQ(CKR_OK, chaps_->EncryptUpdate(session_id_,
                                          SubVector(data, 3, 27),
                                          max_out,
                                          &not_used,
                                          &tmp));
  encrypted2.insert(encrypted2.end(), tmp.begin(), tmp.end());
  tmp.clear();
  EXPECT_EQ(CKR_OK, chaps_->EncryptUpdate(session_id_,
                                          SubVector(data, 30, 18),
                                          max_out,
                                          &not_used,
                                          &tmp));
  encrypted2.insert(encrypted2.end(), tmp.begin(), tmp.end());
  tmp.clear();
  EXPECT_EQ(CKR_OK, chaps_->EncryptFinal(session_id_,
                                         max_out,
                                         &not_used,
                                         &tmp));
  encrypted2.insert(encrypted2.end(), tmp.begin(), tmp.end());
  tmp.clear();
  EXPECT_TRUE(std::equal(encrypted.begin(),
                         encrypted.end(),
                         encrypted2.begin()));
  // Test decrypt.
  EXPECT_EQ(CKR_OK, chaps_->DecryptInit(session_id_,
                                        CKM_AES_ECB,
                                        parameter,
                                        key_handle));
  vector<uint8_t> decrypted;
  EXPECT_EQ(CKR_OK, chaps_->Decrypt(session_id_,
                                    encrypted,
                                    max_out,
                                    &not_used,
                                    &decrypted));
  EXPECT_TRUE(std::equal(data.begin(), data.end(), decrypted.begin()));
  decrypted.clear();
  EXPECT_EQ(CKR_OK, chaps_->DecryptInit(session_id_,
                                        CKM_AES_ECB,
                                        parameter,
                                        key_handle));
  EXPECT_EQ(CKR_OK, chaps_->DecryptUpdate(session_id_,
                                          SubVector(encrypted, 0, 16),
                                          max_out,
                                          &not_used,
                                          &tmp));
  decrypted.insert(decrypted.end(), tmp.begin(), tmp.end());
  tmp.clear();
  EXPECT_EQ(CKR_OK, chaps_->DecryptUpdate(session_id_,
                                          SubVector(encrypted, 16, 17),
                                          max_out,
                                          &not_used,
                                          &tmp));
  decrypted.insert(decrypted.end(), tmp.begin(), tmp.end());
  tmp.clear();
  EXPECT_EQ(CKR_OK, chaps_->DecryptUpdate(session_id_,
                                          SubVector(encrypted, 33, 15),
                                          max_out,
                                          &not_used,
                                          &tmp));
  decrypted.insert(decrypted.end(), tmp.begin(), tmp.end());
  tmp.clear();
  EXPECT_EQ(CKR_OK, chaps_->DecryptFinal(session_id_,
                                         max_out,
                                         &not_used,
                                         &tmp));
  decrypted.insert(decrypted.end(), tmp.begin(), tmp.end());
  tmp.clear();
  EXPECT_TRUE(std::equal(data.begin(), data.end(), decrypted.begin()));
  // Bad arg cases.
  EXPECT_EQ(CKR_OK, chaps_->EncryptInit(session_id_,
                                        CKM_AES_ECB,
                                        parameter,
                                        key_handle));
  EXPECT_EQ(CKR_ARGUMENTS_BAD, chaps_->Encrypt(session_id_, data, max_out, NULL,
                                               NULL));
  EXPECT_EQ(CKR_OK, chaps_->DecryptInit(session_id_,
                                        CKM_AES_ECB,
                                        parameter,
                                        key_handle));
  EXPECT_EQ(CKR_ARGUMENTS_BAD, chaps_->Decrypt(session_id_, data, max_out, NULL,
                                               NULL));
}

TEST_F(TestP11PublicSession, Digest) {
  vector<uint8_t> parameter;
  vector<uint8_t> data(100, 2), digest;
  uint32_t not_used = 0;
  const uint32_t max_out = 100;
  EXPECT_EQ(CKR_OK, chaps_->DigestInit(session_id_, CKM_SHA_1, parameter));
  EXPECT_EQ(CKR_OK,
            chaps_->Digest(session_id_, data, max_out, &not_used, &digest));
  EXPECT_EQ(CKR_OK, chaps_->DigestInit(session_id_, CKM_SHA_1, parameter));
  EXPECT_EQ(CKR_OK, chaps_->DigestUpdate(session_id_, SubVector(data, 0, 10)));
  EXPECT_EQ(CKR_OK, chaps_->DigestUpdate(session_id_, SubVector(data, 10, 90)));
  vector<uint8_t> digest2;
  EXPECT_EQ(CKR_OK,
            chaps_->DigestFinal(session_id_, max_out, &not_used, &digest2));
  EXPECT_TRUE(std::equal(digest.begin(), digest.end(), digest2.begin()));

  // Create a session key.
  CK_OBJECT_CLASS key_class = CKO_SECRET_KEY;
  CK_KEY_TYPE key_type = CKK_GENERIC_SECRET;
  CK_BYTE key_value[100] = {0};
  memcpy(key_value, &data.front(), 100);
  CK_BBOOL false_value = CK_FALSE;
  CK_BBOOL true_value = CK_TRUE;
  CK_ATTRIBUTE key_desc[] = {
    {CKA_CLASS, &key_class, sizeof(key_class)},
    {CKA_KEY_TYPE, &key_type, sizeof(key_type)},
    {CKA_TOKEN, &false_value, sizeof(false_value)},
    {CKA_SIGN, &true_value, sizeof(true_value)},
    {CKA_VERIFY, &true_value, sizeof(true_value)},
    {CKA_VALUE, key_value, sizeof(key_value)}
  };
  vector<uint8_t> key;
  ASSERT_TRUE(SerializeAttributes(key_desc, 6, &key));
  uint32_t key_handle = 0;
  ASSERT_EQ(CKR_OK, chaps_->CreateObject(session_id_, key, &key_handle));
  EXPECT_EQ(CKR_OK, chaps_->DigestInit(session_id_, CKM_SHA_1, parameter));
  EXPECT_EQ(CKR_OK, chaps_->DigestKey(session_id_, key_handle));
  vector<uint8_t> digest3;
  EXPECT_EQ(CKR_OK,
            chaps_->DigestFinal(session_id_, max_out, &not_used, &digest3));
  EXPECT_TRUE(std::equal(digest.begin(), digest.end(), digest3.begin()));
}

TEST_F(TestP11PublicSession, Sign) {
  // Create a session key.
  CK_OBJECT_CLASS key_class = CKO_SECRET_KEY;
  CK_KEY_TYPE key_type = CKK_GENERIC_SECRET;
  CK_BYTE key_value[32] = {0};
  CK_BBOOL false_value = CK_FALSE;
  CK_BBOOL true_value = CK_TRUE;
  CK_ATTRIBUTE key_desc[] = {
    {CKA_CLASS, &key_class, sizeof(key_class)},
    {CKA_KEY_TYPE, &key_type, sizeof(key_type)},
    {CKA_TOKEN, &false_value, sizeof(false_value)},
    {CKA_SIGN, &true_value, sizeof(true_value)},
    {CKA_VERIFY, &true_value, sizeof(true_value)},
    {CKA_VALUE, key_value, sizeof(key_value)}
  };
  vector<uint8_t> key;
  ASSERT_TRUE(SerializeAttributes(key_desc, 6, &key));
  uint32_t key_handle = 0;
  ASSERT_EQ(CKR_OK, chaps_->CreateObject(session_id_, key, &key_handle));
  // Sign / Verify using SHA-1 HMAC.
  vector<uint8_t> parameter;
  EXPECT_EQ(CKR_OK, chaps_->SignInit(session_id_,
                                     CKM_SHA_1_HMAC,
                                     parameter,
                                     key_handle));
  vector<uint8_t> data(100, 2), signature;
  uint32_t not_used = 0;
  const uint32_t max_out = 100;
  EXPECT_EQ(CKR_OK, chaps_->Sign(session_id_,
                                 data,
                                 max_out,
                                 &not_used,
                                 &signature));
  EXPECT_EQ(signature.size(), 20);
  EXPECT_EQ(CKR_OK, chaps_->VerifyInit(session_id_,
                                       CKM_SHA_1_HMAC,
                                       parameter,
                                       key_handle));
  EXPECT_EQ(CKR_OK, chaps_->Verify(session_id_, data, signature));
}

TEST_F(TestP11UserSession, GenerateKey) {
  vector<uint8_t> empty;
  uint32_t key_handle;
  CK_BBOOL false_value = CK_FALSE;
  CK_BBOOL true_value = CK_TRUE;
  CK_ULONG key_length = 32;
  CK_ATTRIBUTE attributes[] = {
    {CKA_TOKEN, &false_value, sizeof(false_value)},
    {CKA_PRIVATE, &true_value, sizeof(true_value)},
    {CKA_ENCRYPT, &true_value, sizeof(true_value)},
    {CKA_DECRYPT, &true_value, sizeof(true_value)},
    {CKA_VALUE_LEN, &key_length, sizeof(key_length)}
  };
  vector<uint8_t> serialized;
  ASSERT_TRUE(SerializeAttributes(attributes, 5, &serialized));
  ASSERT_EQ(CKR_OK, chaps_->GenerateKey(session_id_,
                                        CKM_AES_KEY_GEN,
                                        empty,
                                        serialized,
                                        &key_handle));
  EXPECT_EQ(CKR_OK, chaps_->DestroyObject(session_id_, key_handle));
}

TEST_F(TestP11UserSession, GenerateKeyPair) {
  vector<uint8_t> empty;
  uint32_t public_key, private_key;
  CK_ULONG bits = 1024;
  CK_BYTE e[] = {1, 0, 1};
  CK_BBOOL false_value = CK_FALSE;
  CK_BBOOL true_value = CK_TRUE;
  CK_ATTRIBUTE public_attributes[] = {
    {CKA_ENCRYPT, &true_value, sizeof(true_value)},
    {CKA_VERIFY, &true_value, sizeof(true_value)},
    {CKA_WRAP, &false_value, sizeof(false_value)},
    {CKA_TOKEN, &false_value, sizeof(false_value)},
    {CKA_PRIVATE, &false_value, sizeof(false_value)},
    {CKA_MODULUS_BITS, &bits, sizeof(bits)},
    {CKA_PUBLIC_EXPONENT, e, sizeof(e)}
  };
  CK_BYTE id[] = {'A'};
  CK_ATTRIBUTE private_attributes[] = {
    {CKA_DECRYPT, &true_value, sizeof(true_value)},
    {CKA_SIGN, &true_value, sizeof(true_value)},
    {CKA_UNWRAP, &false_value, sizeof(false_value)},
    {CKA_SENSITIVE, &true_value, sizeof(true_value)},
    {CKA_TOKEN, &false_value, sizeof(false_value)},
    {CKA_PRIVATE, &true_value, sizeof(true_value)},
    {CKA_ID, &id, sizeof(id)},
  };
  vector<uint8_t> public_serialized, private_serialized;
  ASSERT_TRUE(SerializeAttributes(public_attributes, 7, &public_serialized));
  ASSERT_TRUE(SerializeAttributes(private_attributes, 7, &private_serialized));
  ASSERT_EQ(CKR_OK, chaps_->GenerateKeyPair(session_id_,
                                            CKM_RSA_PKCS_KEY_PAIR_GEN,
                                            empty,
                                            public_serialized,
                                            private_serialized,
                                            &public_key,
                                            &private_key));
  EXPECT_EQ(CKR_OK, chaps_->DestroyObject(session_id_, public_key));
  EXPECT_EQ(CKR_OK, chaps_->DestroyObject(session_id_, private_key));
}

TEST_F(TestP11PublicSession, WrapKey) {
  uint32_t not_used;
  vector<uint8_t> empty;
  EXPECT_EQ(CKR_ARGUMENTS_BAD, chaps_->WrapKey(session_id_,
                                               CKM_RSA_PKCS, empty,
                                               0, 1,
                                               1000, &not_used,
                                               NULL));
  EXPECT_EQ(CKR_ARGUMENTS_BAD, chaps_->UnwrapKey(session_id_,
                                                 CKM_RSA_PKCS, empty,
                                                 0, empty,
                                                 empty, NULL));
}

TEST_F(TestP11PublicSession, DeriveKey) {
  vector<uint8_t> empty;
  EXPECT_EQ(CKR_ARGUMENTS_BAD, chaps_->DeriveKey(session_id_,
                                                 CKM_SHA1_KEY_DERIVATION,
                                                 empty,
                                                 1,
                                                 empty,
                                                 NULL));
}

TEST_F(TestP11PublicSession, Random) {
  vector<uint8_t> data(8, 0xAA);
  EXPECT_EQ(CKR_OK, chaps_->SeedRandom(session_id_, data));
  EXPECT_EQ(CKR_OK, chaps_->GenerateRandom(session_id_, 32, &data));
}

}  // namespace chaps

int main(int argc, char** argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
