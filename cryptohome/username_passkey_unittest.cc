// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit tests for UsernamePasskey.

#include "cryptohome/username_passkey.h"

#include <string.h>  // For memset(), memcpy(), memcmp().

#include <base/strings/string_number_conversions.h>
#include <brillo/secure_blob.h>
#include <gtest/gtest.h>
#include <string>

using brillo::SecureBlob;

namespace cryptohome {

const char kFakeUser[] = "fakeuser";
const char kFakePasskey[] = "176c1e698b521373d77ce655d2e56a1d";

// salt must have even number of characters.
const char kFakeSystemSalt[] = "01234567890123456789";

class UsernamePasskeyTest : public ::testing::Test {
 public:
  UsernamePasskeyTest() { }
  virtual ~UsernamePasskeyTest() { }

 private:
  DISALLOW_COPY_AND_ASSIGN(UsernamePasskeyTest);
};

TEST(UsernamePasskeyTest, UsernameTest) {
  char username[80];
  snprintf(username, sizeof(username), "%s%s", kFakeUser, "@gmail.com");
  UsernamePasskey up(username, SecureBlob(kFakePasskey));
  std::string full_username = up.username();
  EXPECT_EQ(0, strcmp(username, full_username.c_str()));
}

TEST(UsernamePasskeyTest, GetObfuscatedUsernameTest) {
  UsernamePasskey up(kFakeUser, SecureBlob(kFakePasskey));

  brillo::SecureBlob fake_salt;
  EXPECT_TRUE(brillo::SecureBlob::HexStringToSecureBlob(kFakeSystemSalt,
                                                        &fake_salt));

  EXPECT_EQ("bb0ae3fcd181eefb861b4f0ee147a316e51d9f04",
            up.GetObfuscatedUsername(fake_salt));
}

TEST(UsernamePasskeyTest, GetPasskeyTest) {
  UsernamePasskey up(kFakeUser, SecureBlob(kFakePasskey));
  SecureBlob passkey;
  up.GetPasskey(&passkey);
  EXPECT_EQ(strlen(kFakePasskey), passkey.size());
  EXPECT_EQ(0, memcmp(kFakePasskey, passkey.data(), passkey.size()));
}

}  // namespace cryptohome
