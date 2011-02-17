// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/owner_key.h"

#include <vector>

#include <base/crypto/rsa_private_key.h>
#include <base/basictypes.h>
#include <base/file_path.h>
#include <base/file_util.h>
#include <base/logging.h>
#include <base/nss_util.h>
#include <base/scoped_temp_dir.h>
#include <gtest/gtest.h>

namespace login_manager {
class OwnerKeyTest : public ::testing::Test {
 public:
  OwnerKeyTest() {}
  virtual ~OwnerKeyTest() {}

  virtual void SetUp() {
    ASSERT_TRUE(tmpdir_.CreateUniqueTempDir());
    ASSERT_TRUE(file_util::CreateTemporaryFileInDir(tmpdir_.path(), &tmpfile_));
    ASSERT_EQ(2, file_util::WriteFile(tmpfile_, "a", 2));
  }

  void StartUnowned() {
    file_util::Delete(tmpfile_, false);
  }

  FilePath tmpfile_;

 private:
  ScopedTempDir tmpdir_;
  DISALLOW_COPY_AND_ASSIGN(OwnerKeyTest);
};

TEST_F(OwnerKeyTest, LoadKey) {
  OwnerKey key(tmpfile_);
  ASSERT_FALSE(key.HaveCheckedDisk());
  ASSERT_FALSE(key.IsPopulated());
  ASSERT_TRUE(key.PopulateFromDiskIfPossible());
  ASSERT_TRUE(key.HaveCheckedDisk());
  ASSERT_TRUE(key.IsPopulated());
}

TEST_F(OwnerKeyTest, NoKeyToLoad) {
  StartUnowned();
  OwnerKey key(tmpfile_);
  ASSERT_FALSE(key.HaveCheckedDisk());
  ASSERT_FALSE(key.IsPopulated());
  ASSERT_TRUE(key.PopulateFromDiskIfPossible());
  ASSERT_TRUE(key.HaveCheckedDisk());
  ASSERT_FALSE(key.IsPopulated());
}

TEST_F(OwnerKeyTest, NoKeyOnDiskAllowSetting) {
  StartUnowned();
  OwnerKey key(tmpfile_);
  ASSERT_FALSE(key.HaveCheckedDisk());
  ASSERT_FALSE(key.IsPopulated());
  ASSERT_TRUE(key.PopulateFromDiskIfPossible());
  ASSERT_TRUE(key.HaveCheckedDisk());
  ASSERT_FALSE(key.IsPopulated());

  std::vector<uint8> fake(1, 1);
  ASSERT_TRUE(key.PopulateFromBuffer(fake));
  ASSERT_TRUE(key.HaveCheckedDisk());
  ASSERT_TRUE(key.IsPopulated());
}

TEST_F(OwnerKeyTest, EnforceDiskCheckFirst) {
  std::vector<uint8> fake(1, 1);

  OwnerKey key(tmpfile_);
  ASSERT_FALSE(key.HaveCheckedDisk());
  ASSERT_FALSE(key.IsPopulated());
  ASSERT_FALSE(key.PopulateFromBuffer(fake));
  ASSERT_FALSE(key.IsPopulated());
  ASSERT_FALSE(key.HaveCheckedDisk());
}

TEST_F(OwnerKeyTest, RefuseToClobberInMemory) {
  std::vector<uint8> fake(1, 1);

  OwnerKey key(tmpfile_);
  ASSERT_FALSE(key.HaveCheckedDisk());
  ASSERT_FALSE(key.IsPopulated());

  ASSERT_TRUE(key.PopulateFromDiskIfPossible());
  ASSERT_TRUE(key.HaveCheckedDisk());
  ASSERT_TRUE(key.IsPopulated());

  ASSERT_FALSE(key.PopulateFromBuffer(fake));
  ASSERT_TRUE(key.HaveCheckedDisk());
  ASSERT_TRUE(key.IsPopulated());
}

TEST_F(OwnerKeyTest, RefuseToClobberOnDisk) {
  OwnerKey key(tmpfile_);
  ASSERT_FALSE(key.HaveCheckedDisk());
  ASSERT_FALSE(key.IsPopulated());

  ASSERT_TRUE(key.PopulateFromDiskIfPossible());
  ASSERT_TRUE(key.HaveCheckedDisk());
  ASSERT_TRUE(key.IsPopulated());

  ASSERT_FALSE(key.Persist());
  ASSERT_TRUE(key.HaveCheckedDisk());
  ASSERT_TRUE(key.IsPopulated());
}

TEST_F(OwnerKeyTest, SignVerify) {
  StartUnowned();
  OwnerKey key(tmpfile_);

  base::EnsureNSSInit();
  base::OpenPersistentNSSDB();
  scoped_ptr<base::RSAPrivateKey> pair(
      base::RSAPrivateKey::CreateSensitive(2048));
  ASSERT_NE(pair.get(), reinterpret_cast<base::RSAPrivateKey*>(NULL));

  ASSERT_TRUE(key.PopulateFromDiskIfPossible());
  ASSERT_TRUE(key.HaveCheckedDisk());
  ASSERT_FALSE(key.IsPopulated());

  std::vector<uint8> to_export;
  ASSERT_TRUE(pair->ExportPublicKey(&to_export));
  ASSERT_TRUE(key.PopulateFromBuffer(to_export));
  ASSERT_TRUE(key.HaveCheckedDisk());
  ASSERT_TRUE(key.IsPopulated());

  std::string data("whatever");
  std::vector<uint8> signature;
  EXPECT_TRUE(key.Sign(data.c_str(), data.length(), &signature));
  EXPECT_TRUE(key.Verify(data.c_str(),
                         data.length(),
                         reinterpret_cast<const char*>(&signature[0]),
                         signature.size()));
}
}  // namespace login_manager
