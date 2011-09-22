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
  printf("Token Info: %s - %s - %s - %s\n", manufacturer.c_str(), model.c_str(),
         label.c_str(), serial_number.c_str());
}

int main(int argc, char** argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
