// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/crypto_rot47.h"

#include <string>

#include <gtest/gtest.h>

using testing::Test;

namespace shill {

namespace {
const char kEmpty[] = "";
const char kPlainText[] = "~{\"Hello world!\" OPQ ['1234']}";
const char kCipherText[] = "OLQw6==@ H@C=5PQ ~!\" ,V`abcV.N";
}  // namespace

class CryptoRot47Test : public Test {
 protected:
  CryptoRot47 crypto_;
};

TEST_F(CryptoRot47Test, GetId) {
  EXPECT_EQ("rot47", crypto_.GetId());
}

TEST_F(CryptoRot47Test, Encrypt) {
  std::string text;
  EXPECT_TRUE(crypto_.Encrypt(kPlainText, &text));
  EXPECT_EQ(kCipherText, text);
  EXPECT_TRUE(crypto_.Encrypt(kEmpty, &text));
  EXPECT_EQ(kEmpty, text);
}

TEST_F(CryptoRot47Test, Decrypt) {
  std::string text;
  EXPECT_TRUE(crypto_.Decrypt(kCipherText, &text));
  EXPECT_EQ(kPlainText, text);
  EXPECT_TRUE(crypto_.Decrypt(kEmpty, &text));
  EXPECT_EQ(kEmpty, text);
}

}  // namespace shill
