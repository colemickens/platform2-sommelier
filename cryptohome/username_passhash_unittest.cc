// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit tests for UsernamePasshash.

#include "cryptohome/username_passhash.h"

#include <gtest/gtest.h>
#include <string.h>  // For memset(), memcpy()
#include <string>

#include "chromeos/utility.h"

namespace cryptohome {
using namespace chromeos;

const char kFakeUser[] = "fakeuser";
const char kFakeHash[] = "176c1e698b521373d77ce655d2e56a1d";

// salt must have even number of characters.
const char kFakeSystemSalt[] = "01234567890123456789";

class UsernamePasshashTest : public ::testing::Test { };

TEST(UsernamePasshashTest, GetFullUsernameTest) {
  char username[80];
  snprintf(username, sizeof(username), "%s%s", kFakeUser, "@gmail.com");
  UsernamePasshash up(username, strlen(username),
                      kFakeHash, strlen(kFakeHash));
  char full_username[80];
  up.GetFullUsername(full_username, sizeof(full_username));
  EXPECT_EQ(0, strcmp(username, full_username));
}

TEST(UsernamePasshashTest, GetPartialUsernameTest) {
  char username[80];
  snprintf(username, sizeof(username), "%s%s", kFakeUser, "@gmail.com");
  UsernamePasshash up(username, strlen(username),
                      kFakeHash, strlen(kFakeHash));
  char partial_username[80];
  up.GetPartialUsername(partial_username, sizeof(partial_username));
  EXPECT_EQ(0, strcmp(kFakeUser, partial_username));
}

TEST(UsernamePasshashTest, GetObfuscatedUsernameTest) {
  UsernamePasshash up(kFakeUser, strlen(kFakeUser),
                      kFakeHash, strlen(kFakeHash));

  Blob fake_salt(AsciiDecode(kFakeSystemSalt));

  EXPECT_EQ("bb0ae3fcd181eefb861b4f0ee147a316e51d9f04",
            up.GetObfuscatedUsername(fake_salt));
}

TEST(UsernamePasshashTest, GetPasshashWeakHashTest) {
  UsernamePasshash up(kFakeUser, strlen(kFakeUser),
                      kFakeHash, strlen(kFakeHash));

  Blob fake_salt(AsciiDecode(kFakeSystemSalt));

  EXPECT_EQ(kFakeHash, up.GetPasswordWeakHash(fake_salt));
}

}  // namespace cryptohome
