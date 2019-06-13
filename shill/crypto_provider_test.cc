// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/crypto_provider.h"

#include <string>

#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <gtest/gtest.h>

#include "shill/crypto_des_cbc.h"
#include "shill/crypto_rot47.h"

using base::FilePath;
using base::ScopedTempDir;
using testing::Test;

namespace shill {

namespace {
const char kTestKey[] = "12345678";
const char kTestIV[] = "abcdefgh";
const char kKeyMatterFile[] = "key-matter-file";
const char kEmptyText[] = "";
const char kPlainText[] = "This is a test!";
const char kROT47Text[] = "rot47:%9:D :D 2 E6DEP";
const char kDESCBCText[] = "des-cbc:02:bKlHDISdHMFc0teQd4mAVrXgwlSj6iA+";
}  // namespace

class CryptoProviderTest : public Test {
 public:
  CryptoProviderTest() = default;

 protected:
  FilePath InitKeyMatterFile(const FilePath& dir);

  CryptoProvider provider_;
};

FilePath CryptoProviderTest::InitKeyMatterFile(const FilePath& dir) {
  FilePath path = dir.Append(kKeyMatterFile);
  std::string matter = std::string(kTestIV) + kTestKey;
  base::WriteFile(path, matter.data(), matter.size());
  return path;
}

TEST_F(CryptoProviderTest, Init) {
  EXPECT_EQ(CryptoProvider::kKeyMatterFile, provider_.key_matter_file_.value());

  provider_.set_key_matter_file(FilePath("/some/non/existent/file"));
  provider_.Init();
  ASSERT_EQ(1, provider_.cryptos_.size());
  EXPECT_EQ("rot47", provider_.cryptos_[0]->GetId());

  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  provider_.set_key_matter_file(InitKeyMatterFile(temp_dir.GetPath()));
  provider_.Init();
  ASSERT_EQ(2, provider_.cryptos_.size());
  EXPECT_EQ("des-cbc", provider_.cryptos_[0]->GetId());
  EXPECT_EQ("rot47", provider_.cryptos_[1]->GetId());

  provider_.set_key_matter_file(FilePath("/other/missing/file"));
  provider_.Init();
  ASSERT_EQ(1, provider_.cryptos_.size());
  EXPECT_EQ("rot47", provider_.cryptos_[0]->GetId());
}

TEST_F(CryptoProviderTest, Encrypt) {
  EXPECT_EQ(kPlainText, provider_.Encrypt(kPlainText));
  EXPECT_EQ(kEmptyText, provider_.Encrypt(kEmptyText));

  provider_.Init();
  EXPECT_EQ(kROT47Text, provider_.Encrypt(kPlainText));

  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  provider_.set_key_matter_file(InitKeyMatterFile(temp_dir.GetPath()));
  provider_.Init();
  EXPECT_EQ(kROT47Text, provider_.Encrypt(kPlainText));
}

TEST_F(CryptoProviderTest, Decrypt) {
  EXPECT_EQ(kPlainText, provider_.Decrypt(kPlainText));
  EXPECT_EQ(kEmptyText, provider_.Decrypt(kEmptyText));

  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  provider_.set_key_matter_file(InitKeyMatterFile(temp_dir.GetPath()));
  provider_.Init();
  EXPECT_EQ(kPlainText, provider_.Decrypt(kROT47Text));
  EXPECT_EQ(kPlainText, provider_.Decrypt(kDESCBCText));
  EXPECT_EQ(kPlainText, provider_.Decrypt(kPlainText));
  EXPECT_EQ(kEmptyText, provider_.Decrypt(kEmptyText));
}

}  // namespace shill
