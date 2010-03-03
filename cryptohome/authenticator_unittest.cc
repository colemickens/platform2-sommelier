// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit tests for UsernamePassword.

#include "cryptohome/authenticator.h"

#include <openssl/sha.h>
#include <string.h>  // For memset(), memcpy()
#include <stdlib.h>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "chromeos/utility.h"
#include "cryptohome/username_passhash.h"
#include "gtest/gtest.h"

namespace cryptohome {
using namespace chromeos;
using namespace file_util;
using std::string;

const char kImageDir[] = "test_image_dir";
const char kFakeUser[] = "testuser@invalid.domain";

class AuthenticatorTest : public ::testing::Test {
  void SetUp() {
    FilePath image_dir(kImageDir);
    FilePath path = image_dir.Append("salt");
    ASSERT_TRUE(PathExists(path)) << path.value() << " does not exist!";

    int64 file_size;
    ASSERT_TRUE(GetFileSize(path, &file_size)) << "Could not get size of "
                                               << path.value();

    char buf[file_size];
    int data_read = ReadFile(path, buf, file_size);
    system_salt_.assign(buf, buf + data_read);
  }

 public:
  string GetWeakHash(const char* password) {
    SHA256_CTX sha_ctx;
    unsigned char md_value[SHA256_DIGEST_LENGTH];

    string system_salt_ascii(AsciiEncode(system_salt_));

    SHA256_Init(&sha_ctx);
    SHA256_Update(&sha_ctx, system_salt_ascii.c_str(),
                  system_salt_ascii.length());
    SHA256_Update(&sha_ctx, password, strlen(password));
    SHA256_Final(md_value, &sha_ctx);

    return AsciiEncode(Blob(md_value, md_value + SHA256_DIGEST_LENGTH / 2));
  }

 private:
  Blob system_salt_;
};

TEST_F(AuthenticatorTest, BadInitTest) {
  // create an authenticator that points to an invalid shadow root
  // and make sure it complains
  Authenticator authn("/dev/null");
  UsernamePasshash up(kFakeUser, strlen(kFakeUser),
                      "zero", 4);

  EXPECT_EQ(false, authn.Init());
  EXPECT_EQ(false, authn.TestAllMasterKeys(up));
}

TEST_F(AuthenticatorTest, GoodDecryptTest0) {
  Authenticator authn(kImageDir);
  string passhash = GetWeakHash("zero");
  UsernamePasshash up(kFakeUser, strlen(kFakeUser),
                      passhash.c_str(), passhash.length());

  EXPECT_EQ(true, authn.Init());
  EXPECT_EQ(true, authn.TestAllMasterKeys(up));
}

TEST_F(AuthenticatorTest, GoodDecryptTest1) {
  Authenticator authn(kImageDir);
  string passhash = GetWeakHash("one");
  UsernamePasshash up(kFakeUser, strlen(kFakeUser),
                      passhash.c_str(), passhash.length());

  EXPECT_EQ(true, authn.Init());
  EXPECT_EQ(true, authn.TestAllMasterKeys(up));
}

TEST_F(AuthenticatorTest, GoodDecryptTest2) {
  Authenticator authn(kImageDir);
  string passhash = GetWeakHash("two");
  UsernamePasshash up(kFakeUser, strlen(kFakeUser),
                      passhash.c_str(), passhash.length());

  EXPECT_EQ(true, authn.Init());
  EXPECT_EQ(true, authn.TestAllMasterKeys(up));
}

TEST_F(AuthenticatorTest, BadDecryptTest) {
  Authenticator authn(kImageDir);
  string passhash = GetWeakHash("bogus");
  UsernamePasshash up(kFakeUser, strlen(kFakeUser),
                      passhash.c_str(), passhash.length());

  EXPECT_EQ(true, authn.Init());
  EXPECT_EQ(false, authn.TestAllMasterKeys(up));
}

} // namespace cryptohome
