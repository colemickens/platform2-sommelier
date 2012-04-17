// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/nss.h"

#include <gtest/gtest.h>

#include "shill/mock_glib.h"

using std::vector;
using testing::_;
using testing::NotNull;
using testing::Return;
using testing::SetArgumentPointee;

namespace shill {

class NSSTest : public testing::Test {
 public:
  NSSTest() : nss_(NSS::GetInstance()) {
    nss_->glib_ = &glib_;
    test_id_.push_back(0x1a);
    test_id_.push_back(0x2b);
  }

 protected:
  vector<char> test_id_;
  MockGLib glib_;
  NSS *nss_;
};

namespace {
MATCHER_P(GetCertfileArgv, type, "") {
  if (!arg || !arg[0] || !arg[1] || !arg[2] || !arg[3] || arg[4]) {
    return false;
  }
  if (strcmp(type, arg[2])) {
    return false;
  }
  if (strcmp(arg[3], "/tmp/nss-cert.1a2b")) {
    return false;
  }
  return true;
}
}  // namespace

TEST_F(NSSTest, GetCertfile) {
  EXPECT_CALL(glib_,
              SpawnSync(_,
                        GetCertfileArgv("pem"), NotNull(), _, _, _, _, _, _, _))
      .WillOnce(Return(false))
      .WillOnce(DoAll(SetArgumentPointee<8>(1), Return(true)))
      .WillOnce(DoAll(SetArgumentPointee<8>(0), Return(true)));
  EXPECT_TRUE(nss_->GetCertfile("foo", test_id_, "pem").empty());
  EXPECT_TRUE(nss_->GetCertfile("foo", test_id_, "pem").empty());
  EXPECT_FALSE(nss_->GetCertfile("foo", test_id_, "pem").empty());
}

TEST_F(NSSTest, GetPEMCertfile) {
  EXPECT_CALL(glib_,
              SpawnSync(_, GetCertfileArgv("pem"), _, _, _, _, _, _, _, _))
      .WillOnce(DoAll(SetArgumentPointee<8>(0), Return(true)));
  EXPECT_FALSE(nss_->GetPEMCertfile("foo", test_id_).empty());
}

TEST_F(NSSTest, GetDERCertfile) {
  EXPECT_CALL(glib_,
              SpawnSync(_, GetCertfileArgv("der"), _, _, _, _, _, _, _, _))
      .WillOnce(DoAll(SetArgumentPointee<8>(0), Return(true)));
  EXPECT_FALSE(nss_->GetDERCertfile("foo", test_id_).empty());
}

}  // namespace shill
