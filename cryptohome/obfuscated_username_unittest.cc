// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/obfuscated_username.h"

#include <string>

#include <base/strings/string_number_conversions.h>
#include <brillo/secure_blob.h>
#include <gtest/gtest.h>

namespace cryptohome {

const char kUsername[] = "fakeuser";

// The salt must have even number of characters.
const char kSystemSaltHex[] = "01234567890123456789";

TEST(ObfuscatedUsername, BuildObfuscatedUsername) {
  brillo::SecureBlob system_salt;
  EXPECT_TRUE(brillo::SecureBlob::HexStringToSecureBlob(kSystemSaltHex,
                                                        &system_salt));
  EXPECT_EQ("bb0ae3fcd181eefb861b4f0ee147a316e51d9f04",
            BuildObfuscatedUsername(kUsername, system_salt));
}

}  // namespace cryptohome
