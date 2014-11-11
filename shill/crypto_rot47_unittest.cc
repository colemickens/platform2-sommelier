// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/crypto_rot47.h"

#include <string>

#include <gtest/gtest.h>

using std::string;
using testing::Test;

namespace shill {

namespace {
const char kEmpty[] = "";
const char kPlainText[] = "~{\"Hello world!\" OPQ ['1234']}";
const char kCipherText[] = "OLQw6==@ H@C=5PQ ~!\" ,V`abcV.N";
}  // namespace

class CryptoROT47Test : public Test {
 protected:
  CryptoROT47 crypto_;
};

TEST_F(CryptoROT47Test, GetID) {
  EXPECT_EQ(CryptoROT47::kID, crypto_.GetID());
}

TEST_F(CryptoROT47Test, Encrypt) {
  string text;
  EXPECT_TRUE(crypto_.Encrypt(kPlainText, &text));
  EXPECT_EQ(kCipherText, text);
  EXPECT_TRUE(crypto_.Encrypt(kEmpty, &text));
  EXPECT_EQ(kEmpty, text);
}

TEST_F(CryptoROT47Test, Decrypt) {
  string text;
  EXPECT_TRUE(crypto_.Decrypt(kCipherText, &text));
  EXPECT_EQ(kPlainText, text);
  EXPECT_TRUE(crypto_.Decrypt(kEmpty, &text));
  EXPECT_EQ(kEmpty, text);
}

}  // namespace shill
