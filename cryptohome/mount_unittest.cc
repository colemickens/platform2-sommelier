// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit tests for Mount.

#include "cryptohome/mount.h"

#include <openssl/sha.h>
#include <pwd.h>
#include <string.h>  // For memset(), memcpy()
#include <stdlib.h>
#include <sys/types.h>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "chromeos/utility.h"
#include "cryptohome/username_passkey.h"
#include "gtest/gtest.h"

namespace cryptohome {
using namespace chromeos;
using namespace file_util;
using std::string;

const char kImageDir[] = "test_image_dir";
const char kSkelDir[] = "test_image_dir/skel";
const char kFakeUser[] = "testuser@invalid.domain";
const char kFakeUser2[] = "testuser2@invalid.domain";
const char kFakeUser3[] = "testuser3@invalid.domain";

class MountTest : public ::testing::Test {
  void SetUp() {
    FilePath image_dir(kImageDir);
    FilePath path = image_dir.Append("salt");
    ASSERT_TRUE(PathExists(path)) << path.value() << " does not exist!";

    int64 file_size;
    ASSERT_TRUE(GetFileSize(path, &file_size)) << "Could not get size of "
                                               << path.value();

    char* buf = new char[file_size];
    int data_read = ReadFile(path, buf, file_size);
    system_salt_.assign(buf, buf + data_read);
    delete buf;
  }

 public:

 protected:
  // Protected for trivial access
  Blob system_salt_;

 private:
};

TEST_F(MountTest, BadInitTest) {
  // create a Mount instance that points to a bad shadow root
  Mount mount(cryptohome::kDefaultSharedUser,
              cryptohome::kDefaultEntropySource,
              cryptohome::kDefaultHomeDir,
              "/dev/null",
              kSkelDir);
  UsernamePasskey up = UsernamePasskey::FromUsernamePassword(kFakeUser,
                                                             "zero",
                                                             system_salt_);

  EXPECT_EQ(false, mount.Init());
  EXPECT_EQ(false, mount.TestCredentials(up));
}

TEST_F(MountTest, GoodDecryptTest0) {
  // create a Mount instance that points to a good shadow root, test that it
  // properly authenticates against the first key
  Mount mount(cryptohome::kDefaultSharedUser,
              cryptohome::kDefaultEntropySource,
              cryptohome::kDefaultHomeDir,
              kImageDir,
              kSkelDir);
  UsernamePasskey up = UsernamePasskey::FromUsernamePassword(kFakeUser,
                                                             "zero",
                                                             system_salt_);

  EXPECT_EQ(true, mount.Init());
  EXPECT_EQ(true, mount.TestCredentials(up));
}

TEST_F(MountTest, GoodDecryptTest1) {
  // create a Mount instance that points to a good shadow root, test that it
  // properly authenticates against the second key
  Mount mount(cryptohome::kDefaultSharedUser,
              cryptohome::kDefaultEntropySource,
              cryptohome::kDefaultHomeDir,
              kImageDir,
              kSkelDir);
  UsernamePasskey up = UsernamePasskey::FromUsernamePassword(kFakeUser,
                                                             "one",
                                                             system_salt_);

  EXPECT_EQ(true, mount.Init());
  EXPECT_EQ(true, mount.TestCredentials(up));
}

TEST_F(MountTest, GoodDecryptTest2) {
  // create a Mount instance that points to a good shadow root, test that it
  // properly authenticates against the third key
  Mount mount(cryptohome::kDefaultSharedUser,
              cryptohome::kDefaultEntropySource,
              cryptohome::kDefaultHomeDir,
              kImageDir,
              kSkelDir);
  UsernamePasskey up = UsernamePasskey::FromUsernamePassword(kFakeUser,
                                                             "two",
                                                             system_salt_);

  EXPECT_EQ(true, mount.Init());
  EXPECT_EQ(true, mount.TestCredentials(up));
}

TEST_F(MountTest, BadDecryptTest) {
  // create a Mount instance that points to a good shadow root, test that it
  // properly denies access with a bad passkey
  Mount mount(cryptohome::kDefaultSharedUser,
              cryptohome::kDefaultEntropySource,
              cryptohome::kDefaultHomeDir,
              kImageDir,
              kSkelDir);
  UsernamePasskey up = UsernamePasskey::FromUsernamePassword(kFakeUser,
                                                             "bogus",
                                                             system_salt_);

  EXPECT_EQ(true, mount.Init());
  EXPECT_EQ(false, mount.TestCredentials(up));
}

TEST_F(MountTest, CreateCryptohomeTest) {
  // creates a cryptohome
  Mount mount(cryptohome::kDefaultSharedUser,
              cryptohome::kDefaultEntropySource,
              cryptohome::kDefaultHomeDir,
              kImageDir,
              kSkelDir);
  // Don't set the vault ownership--this will fail
  mount.set_set_vault_ownership(false);
  UsernamePasskey up = UsernamePasskey::FromUsernamePassword(kFakeUser2,
                                                             "one",
                                                             system_salt_);

  EXPECT_EQ(true, mount.Init());
  EXPECT_EQ(true, mount.CreateCryptohome(up, 0));

  FilePath image_dir(kImageDir);
  FilePath user_path = image_dir.Append(up.GetObfuscatedUsername(system_salt_));
  FilePath key_path = user_path.Append("master.0");
  FilePath vault_path = user_path.Append("vault");
  FilePath skel_testfile_path = user_path.Append("sub_path/.testfile");

  EXPECT_EQ(true, file_util::PathExists(key_path));
  EXPECT_EQ(true, file_util::PathExists(vault_path));
}

TEST_F(MountTest, SystemSaltTest) {
  // checks that cryptohome reads the system salt
  Mount mount(cryptohome::kDefaultSharedUser,
              cryptohome::kDefaultEntropySource,
              cryptohome::kDefaultHomeDir,
              kImageDir,
              kSkelDir);

  EXPECT_EQ(true, mount.Init());
  chromeos::Blob system_salt = mount.GetSystemSalt();
  EXPECT_EQ(true, (system_salt.size() == system_salt_.size()));
  EXPECT_EQ(0, memcmp(&system_salt[0], &system_salt_[0],
                         system_salt.size()));
}

} // namespace cryptohome
