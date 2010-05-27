// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit tests for UsernamePasskey.

#include "cryptohome/username_passkey.h"

#include <gtest/gtest.h>
#include <string.h>  // For memset(), memcpy()
#include <string>

#include "chromeos/utility.h"

namespace cryptohome {
using namespace chromeos;

const char kFakeUser[] = "fakeuser";
const char kFakePasskey[] = "176c1e698b521373d77ce655d2e56a1d";

// salt must have even number of characters.
const char kFakeSystemSalt[] = "01234567890123456789";

class UsernamePasskeyTest : public ::testing::Test { };

TEST(UsernamePasskeyTest, GetFullUsernameTest) {
  char username[80];
  snprintf(username, sizeof(username), "%s%s", kFakeUser, "@gmail.com");
  UsernamePasskey up(username, strlen(username),
                      kFakePasskey, strlen(kFakePasskey));
  char full_username[80];
  up.GetFullUsername(full_username, sizeof(full_username));
  EXPECT_EQ(0, strcmp(username, full_username));
}

TEST(UsernamePasskeyTest, GetPartialUsernameTest) {
  char username[80];
  snprintf(username, sizeof(username), "%s%s", kFakeUser, "@gmail.com");
  UsernamePasskey up(username, strlen(username),
                      kFakePasskey, strlen(kFakePasskey));
  char partial_username[80];
  up.GetPartialUsername(partial_username, sizeof(partial_username));
  EXPECT_EQ(0, strcmp(kFakeUser, partial_username));
}

TEST(UsernamePasskeyTest, GetObfuscatedUsernameTest) {
  UsernamePasskey up(kFakeUser, strlen(kFakeUser),
                      kFakePasskey, strlen(kFakePasskey));

  Blob fake_salt(AsciiDecode(kFakeSystemSalt));

  EXPECT_EQ("bb0ae3fcd181eefb861b4f0ee147a316e51d9f04",
            up.GetObfuscatedUsername(fake_salt));
}

TEST(UsernamePasskeyTest, GetPasskeyTest) {
  UsernamePasskey up(kFakeUser, strlen(kFakeUser),
                      kFakePasskey, strlen(kFakePasskey));
  Blob passkey = up.GetPasskey();
  EXPECT_EQ(strlen(kFakePasskey), passkey.size());
  EXPECT_EQ(0, memcmp(kFakePasskey, &passkey[0], passkey.size()));
}

}  // cryptohome
