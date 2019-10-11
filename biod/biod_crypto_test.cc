// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include <gtest/gtest.h>

#include "biod/biod_crypto.h"
#include "biod/biod_crypto_test_data.h"

namespace biod {
namespace {

using crypto_test_data::kFakePositiveMatchSecret1;
using crypto_test_data::kFakeValidationValue1;
using crypto_test_data::kUserID;

TEST(BiodCryptoTest, ComputeValidationValue) {
  brillo::SecureBlob secret(kFakePositiveMatchSecret1);
  std::vector<uint8_t> result;
  EXPECT_TRUE(BiodCrypto::ComputeValidationValue(secret, kUserID, &result));
  EXPECT_EQ(result, kFakeValidationValue1);
}

TEST(BiodCryptoTest, ComputeValidationValue_InvalidUserId) {
  brillo::SecureBlob secret(kFakePositiveMatchSecret1);
  std::string invalid_user_id = "nothex";
  std::vector<uint8_t> result;
  EXPECT_FALSE(
      BiodCrypto::ComputeValidationValue(secret, invalid_user_id, &result));
  EXPECT_TRUE(result.empty());
}

}  // namespace
}  // namespace biod
