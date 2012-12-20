// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/nss.h"

#include <gtest/gtest.h>

#include "shill/mock_minijail.h"

using std::vector;
using testing::_;
using testing::ElementsAre;
using testing::IsNull;
using testing::Return;
using testing::SetArgumentPointee;
using testing::StrEq;

namespace shill {

class NSSTest : public testing::Test {
 public:
  NSSTest() :
      nss_(NSS::GetInstance()) {
    nss_->minijail_ = &minijail_;
    test_id_.push_back(0x1a);
    test_id_.push_back(0x2b);
  }

 protected:
  vector<char> test_id_;
  NSS *nss_;
  MockMinijail minijail_;
};

TEST_F(NSSTest, GetCertfile) {
  EXPECT_CALL(minijail_, DropRoot(_, StrEq("chronos"))).Times(3);
  EXPECT_CALL(minijail_,
              RunSyncAndDestroy(_,
                                ElementsAre(_, _,
                                            StrEq("pem"),
                                            StrEq("/tmp/nss-cert.1a2b"),
                                            IsNull()),
                                _))
      .WillOnce(Return(false))
      .WillOnce(DoAll(SetArgumentPointee<2>(1), Return(true)))
      .WillOnce(DoAll(SetArgumentPointee<2>(0), Return(true)));
  EXPECT_TRUE(nss_->GetCertfile("foo", test_id_, "pem").empty());
  EXPECT_TRUE(nss_->GetCertfile("foo", test_id_, "pem").empty());
  EXPECT_FALSE(nss_->GetCertfile("foo", test_id_, "pem").empty());
}

TEST_F(NSSTest, GetPEMCertfile) {
  EXPECT_CALL(minijail_, DropRoot(_, StrEq("chronos")));
  EXPECT_CALL(minijail_,
              RunSyncAndDestroy(_,
                                ElementsAre(_, _,
                                            StrEq("pem"),
                                            StrEq("/tmp/nss-cert.1a2b"),
                                            IsNull()),
                                _))
      .WillOnce(DoAll(SetArgumentPointee<2>(0), Return(true)));
  EXPECT_FALSE(nss_->GetPEMCertfile("foo", test_id_).empty());
}

TEST_F(NSSTest, GetDERCertfile) {
  EXPECT_CALL(minijail_, DropRoot(_, StrEq("chronos")));
  EXPECT_CALL(minijail_,
              RunSyncAndDestroy(_,
                                ElementsAre(_, _,
                                            StrEq("der"),
                                            StrEq("/tmp/nss-cert.1a2b"),
                                            IsNull()),
                                _))
      .WillOnce(DoAll(SetArgumentPointee<2>(0), Return(true)));
  EXPECT_FALSE(nss_->GetDERCertfile("foo", test_id_).empty());
}

}  // namespace shill
