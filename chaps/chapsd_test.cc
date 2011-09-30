// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include <gflags/gflags.h>
#include <gtest/gtest.h>

#include "chaps/chaps_interface.h"
#include "chaps/chaps_proxy.h"
#include "chaps/chaps_service_redirect.h"

using std::string;
using std::vector;

DEFINE_bool(use_dbus, false, "");

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
  }
  scoped_ptr<chaps::ChapsInterface> chaps_;
};

TEST_F(TestP11, SlotList) {
  vector<uint32_t> slot_list;
  uint32_t result = chaps_->GetSlotList(false, &slot_list);
  EXPECT_EQ(CKR_OK, result);
  EXPECT_LT(0, slot_list.size());
  printf("Slots: ");
  for (size_t i = 0; i < slot_list.size(); i++) {
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

TEST_F(TestP11, InitToken) {
  string pin = "test";
  string label = "test";
  EXPECT_NE(CKR_OK, chaps_->InitToken(0, &pin, label));
  EXPECT_NE(CKR_OK, chaps_->InitToken(0, NULL, label));
}

TEST_F(TestP11, InitPIN) {
  string pin = "test";
  EXPECT_NE(CKR_OK, chaps_->InitPIN(0, &pin));
  EXPECT_NE(CKR_OK, chaps_->InitPIN(0, NULL));
}

TEST_F(TestP11, SetPIN) {
  string pin = "test";
  string pin2 = "test2";
  EXPECT_NE(CKR_OK, chaps_->SetPIN(0, &pin, &pin2));
  EXPECT_NE(CKR_OK, chaps_->SetPIN(0, NULL, NULL));
}

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

int main(int argc, char** argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
