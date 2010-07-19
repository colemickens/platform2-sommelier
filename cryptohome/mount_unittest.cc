// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit tests for Mount.

#include "mount.h"

#include <openssl/sha.h>
#include <pwd.h>
#include <string.h>  // For memset(), memcpy()
#include <stdlib.h>
#include <sys/types.h>

#include <base/file_path.h>
#include <base/file_util.h>
#include <base/logging.h>
#include <chromeos/utility.h>
#include <gtest/gtest.h>

#include "crypto.h"
#include "secure_blob.h"
#include "username_passkey.h"
#include "mock_user_session.h"

namespace cryptohome {
using std::string;
using ::testing::Return;
using ::testing::_;

const char kImageDir[] = "test_image_dir";
const char kSkelDir[] = "test_image_dir/skel";
const char kFakeUser[] = "testuser@invalid.domain";
const char kFakeUser2[] = "testuser2@invalid.domain";
const char kFakeUser3[] = "testuser3@invalid.domain";

class MountTest : public ::testing::Test {
 public:
  MountTest() { }
  virtual ~MountTest() { }

  void SetUp() {
    FilePath image_dir(kImageDir);
    FilePath path = image_dir.Append("salt");
    ASSERT_TRUE(file_util::PathExists(path)) << path.value()
                                             << " does not exist!";

    int64 file_size;
    ASSERT_TRUE(file_util::GetFileSize(path, &file_size))
                << "Could not get size of "
                << path.value();

    char* buf = new char[file_size];
    int data_read = file_util::ReadFile(path, buf, file_size);
    system_salt_.assign(buf, buf + data_read);
    delete buf;
  }

 protected:
  // Protected for trivial access
  chromeos::Blob system_salt_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MountTest);
};

TEST_F(MountTest, BadInitTest) {
  // create a Mount instance that points to a bad shadow root
  Mount mount;
  mount.set_shadow_root("/dev/null");
  mount.set_skel_source(kSkelDir);

  cryptohome::SecureBlob passkey;
  cryptohome::Crypto::PasswordToPasskey("zero", system_salt_, &passkey);
  UsernamePasskey up(kFakeUser, passkey);

  EXPECT_EQ(false, mount.Init());
  EXPECT_EQ(false, mount.TestCredentials(up));
}

TEST_F(MountTest, GoodDecryptTest) {
  // create a Mount instance that points to a good shadow root, test that it
  // properly authenticates against the first key.
  // Note that the credentials used in this test are pre-created using an
  // external script, and that script creates an old-style vault keyset.  So
  // this test actually verifies that we can still use old vault keysets.
  Mount mount;
  mount.set_shadow_root(kImageDir);
  mount.set_skel_source(kSkelDir);

  cryptohome::SecureBlob passkey;
  cryptohome::Crypto::PasswordToPasskey("zero", system_salt_, &passkey);
  UsernamePasskey up(kFakeUser, passkey);

  EXPECT_EQ(true, mount.Init());
  EXPECT_EQ(true, mount.TestCredentials(up));
}

TEST_F(MountTest, GoodReDecryptTest) {
  // create a Mount instance that points to a good shadow root, test that it
  // properly authenticates against the first key
  Mount mount;
  mount.set_shadow_root(kImageDir);
  mount.set_skel_source(kSkelDir);

  cryptohome::SecureBlob passkey;
  cryptohome::Crypto::PasswordToPasskey("zero", system_salt_, &passkey);
  UsernamePasskey up(kFakeUser, passkey);

  EXPECT_EQ(true, mount.Init());
  EXPECT_EQ(true, mount.TestCredentials(up));
}

TEST_F(MountTest, CurrentCredentialsTest) {
  // create a Mount instance that points to a good shadow root, test that it
  // properly authenticates against the first key
  Mount mount;
  mount.set_shadow_root(kImageDir);
  mount.set_skel_source(kSkelDir);

  cryptohome::SecureBlob passkey;
  cryptohome::Crypto::PasswordToPasskey("zero", system_salt_, &passkey);
  UsernamePasskey up(kFakeUser, passkey);

  EXPECT_EQ(true, mount.Init());

  MockUserSession user_session;
  Crypto crypto;
  user_session.Init(&crypto);
  user_session.SetUser(up);
  mount.set_current_user(&user_session);

  EXPECT_CALL(user_session, CheckUser(_))
      .WillOnce(Return(true));
  EXPECT_CALL(user_session, Verify(_))
      .WillOnce(Return(true));

  EXPECT_EQ(true, mount.TestCredentials(up));
}

TEST_F(MountTest, BadDecryptTest) {
  // create a Mount instance that points to a good shadow root, test that it
  // properly denies access with a bad passkey
  Mount mount;
  mount.set_shadow_root(kImageDir);
  mount.set_skel_source(kSkelDir);

  cryptohome::SecureBlob passkey;
  cryptohome::Crypto::PasswordToPasskey("bogus", system_salt_, &passkey);
  UsernamePasskey up(kFakeUser, passkey);

  EXPECT_EQ(true, mount.Init());
  EXPECT_EQ(false, mount.TestCredentials(up));
}

TEST_F(MountTest, CreateCryptohomeTest) {
  // creates a cryptohome
  Mount mount;
  mount.set_shadow_root(kImageDir);
  mount.set_skel_source(kSkelDir);
  mount.set_set_vault_ownership(false);

  cryptohome::SecureBlob passkey;
  cryptohome::Crypto::PasswordToPasskey("one", system_salt_, &passkey);
  UsernamePasskey up(kFakeUser2, passkey);

  EXPECT_EQ(true, mount.Init());
  EXPECT_EQ(true, mount.CreateCryptohome(up));

  FilePath image_dir(kImageDir);
  FilePath user_path = image_dir.Append(up.GetObfuscatedUsername(system_salt_));
  FilePath key_path = user_path.Append("master.0");
  FilePath vault_path = user_path.Append("vault");
  FilePath skel_testfile_path = user_path.Append("sub_path/.testfile");

  EXPECT_EQ(true, file_util::PathExists(key_path));
  EXPECT_EQ(true, file_util::PathExists(vault_path));
}

TEST_F(MountTest, TestNewCredentials) {
  // Tests the newly-created cryptohome
  Mount mount;
  mount.set_shadow_root(kImageDir);
  mount.set_skel_source(kSkelDir);

  cryptohome::SecureBlob passkey;
  cryptohome::Crypto::PasswordToPasskey("one", system_salt_, &passkey);
  UsernamePasskey up(kFakeUser2, passkey);

  EXPECT_EQ(true, mount.Init());
  EXPECT_EQ(true, mount.TestCredentials(up));
}

TEST_F(MountTest, SystemSaltTest) {
  // checks that cryptohome reads the system salt
  Mount mount;
  mount.set_shadow_root(kImageDir);
  mount.set_skel_source(kSkelDir);

  EXPECT_EQ(true, mount.Init());
  chromeos::Blob system_salt;
  mount.GetSystemSalt(&system_salt);
  EXPECT_EQ(true, (system_salt.size() == system_salt_.size()));
  EXPECT_EQ(0, memcmp(&system_salt[0], &system_salt_[0],
                         system_salt.size()));
}

} // namespace cryptohome
