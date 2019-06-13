// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/crypto_des_cbc.h"

#include <string>

#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <gtest/gtest.h>

using base::FilePath;
using std::string;
using testing::Test;

namespace shill {

namespace {
const char kTestKey[] = "12345678";
const char kTestIV[] = "abcdefgh";
const char kEmptyPlain[] = "";
const char kEmptyCipher[] = "02:4+O1a2KJVRM=";
const char kEmptyCipherNoSentinel[] = "02:lNRDa8O1tpM=";
const char kPlainText[] = "Hello world! ~123";
const char kCipherText[] = "02:MbxzeBqK3HVeS3xfjyhbe47Xx+szYgOp";
const char kPlainVersion1[] = "This is a test!";
const char kCipherVersion1[] = "bKlHDISdHMFfmfgBTT5I0w==";
}  // namespace

class CryptoDesCbcTest : public Test {
 public:
  CryptoDesCbcTest() = default;

 protected:
  CryptoDesCbc crypto_;
};

TEST_F(CryptoDesCbcTest, GetId) {
  EXPECT_EQ("des-cbc", crypto_.GetId());
}

TEST_F(CryptoDesCbcTest, LoadKeyMatter) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const char kKeyMatterFile[] = "key-matter-file";
  FilePath key_matter = temp_dir.GetPath().Append(kKeyMatterFile);

  EXPECT_FALSE(crypto_.LoadKeyMatter(key_matter));
  EXPECT_TRUE(crypto_.key().empty());
  EXPECT_TRUE(crypto_.iv().empty());

  string matter = string(kTestIV) + kTestKey;

  base::WriteFile(key_matter, matter.data(), matter.size() - 1);
  EXPECT_FALSE(crypto_.LoadKeyMatter(key_matter));
  EXPECT_TRUE(crypto_.key().empty());
  EXPECT_TRUE(crypto_.iv().empty());

  base::WriteFile(key_matter, matter.data(), matter.size());
  EXPECT_TRUE(crypto_.LoadKeyMatter(key_matter));
  EXPECT_EQ(kTestKey, string(crypto_.key().begin(), crypto_.key().end()));
  EXPECT_EQ(kTestIV, string(crypto_.iv().begin(), crypto_.iv().end()));

  const char kKey2[] = "ABCDEFGH";
  const char kIV2[] = "87654321";
  matter = string("X") + kIV2 + kKey2;

  base::WriteFile(key_matter, matter.data(), matter.size());
  EXPECT_TRUE(crypto_.LoadKeyMatter(key_matter));
  EXPECT_EQ(kKey2, string(crypto_.key().begin(), crypto_.key().end()));
  EXPECT_EQ(kIV2, string(crypto_.iv().begin(), crypto_.iv().end()));

  base::WriteFile(key_matter, " ", 1);
  EXPECT_FALSE(crypto_.LoadKeyMatter(key_matter));
  EXPECT_TRUE(crypto_.key().empty());
  EXPECT_TRUE(crypto_.iv().empty());
}

TEST_F(CryptoDesCbcTest, Encrypt) {
  crypto_.key_.assign(kTestKey, kTestKey + strlen(kTestKey));
  crypto_.iv_.assign(kTestIV, kTestIV + strlen(kTestIV));

  string ciphertext;
  EXPECT_FALSE(crypto_.Encrypt(kPlainText, &ciphertext));
}

TEST_F(CryptoDesCbcTest, Decrypt) {
  crypto_.key_.assign(kTestKey, kTestKey + strlen(kTestKey));
  crypto_.iv_.assign(kTestIV, kTestIV + strlen(kTestIV));

  string plaintext;
  EXPECT_TRUE(crypto_.Decrypt(kEmptyCipher, &plaintext));
  EXPECT_EQ(kEmptyPlain, plaintext);
  EXPECT_TRUE(crypto_.Decrypt(kCipherText, &plaintext));
  EXPECT_EQ(kPlainText, plaintext);
  EXPECT_TRUE(crypto_.Decrypt(kCipherVersion1, &plaintext));
  EXPECT_EQ(kPlainVersion1, plaintext);

  EXPECT_FALSE(crypto_.Decrypt("random", &plaintext));
  EXPECT_FALSE(crypto_.Decrypt("02:random", &plaintext));
  EXPECT_FALSE(crypto_.Decrypt("~", &plaintext));
  EXPECT_FALSE(crypto_.Decrypt("02:~", &plaintext));
  EXPECT_FALSE(crypto_.Decrypt(kEmptyPlain, &plaintext));
  EXPECT_FALSE(crypto_.Decrypt(kEmptyCipherNoSentinel, &plaintext));

  // echo -n 12345678 | base64
  EXPECT_FALSE(crypto_.Decrypt("MTIzNDU2Nzg=", &plaintext));
}

}  // namespace shill
