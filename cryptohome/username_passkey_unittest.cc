// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit tests for UsernamePasskey.

#include "username_passkey.h"

#include <string.h>  // For memset(), memcpy()

#include <chromeos/utility.h>
#include <gtest/gtest.h>
#include <string>

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
  UsernamePasskey up(username, SecureBlob(kFakePasskey, strlen(kFakePasskey)));
  std::string full_username = up.username();
  EXPECT_EQ(0, strcmp(username, full_username.c_str()));
}

TEST(UsernamePasskeyTest, GetObfuscatedUsernameTest) {
  UsernamePasskey up(kFakeUser, SecureBlob(kFakePasskey, strlen(kFakePasskey)));

  chromeos::Blob fake_salt(chromeos::AsciiDecode(kFakeSystemSalt));

  EXPECT_EQ("bb0ae3fcd181eefb861b4f0ee147a316e51d9f04",
            up.GetObfuscatedUsername(fake_salt));
}

TEST(UsernamePasskeyTest, GetPasskeyTest) {
  UsernamePasskey up(kFakeUser, SecureBlob(kFakePasskey, strlen(kFakePasskey)));
  SecureBlob passkey;
  up.GetPasskey(&passkey);
  EXPECT_EQ(strlen(kFakePasskey), passkey.size());
  EXPECT_EQ(0, chromeos::SafeMemcmp(kFakePasskey, &passkey[0], passkey.size()));
}

}  // namespace cryptohome
