// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/nss_util.h"

#include <base/memory/scoped_ptr.h>
#include <crypto/nss_util.h>
#include <crypto/rsa_private_key.h>
#include <gtest/gtest.h>

using crypto::RSAPrivateKey;

namespace login_manager {
class NssUtilTest : public ::testing::Test {
 public:
  NssUtilTest() : util_(NssUtil::Create()) {}
  virtual ~NssUtilTest() {}

  virtual void SetUp() {
    ASSERT_TRUE(util_->OpenUserDB());
  }

  scoped_ptr<NssUtil> util_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NssUtilTest);
};

TEST_F(NssUtilTest, FindFromPublicKey) {
  // Create a keypair, which will put the keys in the user's NSSDB.
  scoped_ptr<RSAPrivateKey> key_pair(RSAPrivateKey::CreateSensitive(256));
  ASSERT_NE(key_pair.get(), reinterpret_cast<RSAPrivateKey*>(NULL));

  std::vector<uint8> public_key;
  ASSERT_TRUE(key_pair->ExportPublicKey(&public_key));

  EXPECT_TRUE(util_->CheckPublicKeyBlob(public_key));

  EXPECT_NE(util_->GetPrivateKey(public_key),
            reinterpret_cast<RSAPrivateKey*>(NULL));
}

TEST_F(NssUtilTest, RejectBadPublicKey) {
  std::vector<uint8> public_key(10, 'a');
  EXPECT_FALSE(util_->CheckPublicKeyBlob(public_key));
}

}  // namespace login_manager
