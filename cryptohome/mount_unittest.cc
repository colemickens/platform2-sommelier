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
#include "make_tests.h"
#include "mock_user_session.h"
#include "vault_keyset.h"
#include "vault_keyset.pb.h"

namespace cryptohome {
using std::string;
using ::testing::Return;
using ::testing::_;

const char kImageDir[] = "test_image_dir";
const char kSkelDir[] = "test_image_dir/skel";

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

  bool LoadSerializedKeyset(const std::string& key_path,
                            cryptohome::SerializedVaultKeyset* serialized) {
    cryptohome::SecureBlob contents;
    if (!cryptohome::Mount::LoadFileBytes(FilePath(key_path), &contents)) {
      return false;
    }
    return serialized->ParseFromArray(
        static_cast<const unsigned char*>(&contents[0]), contents.size());
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
  cryptohome::Crypto::PasswordToPasskey(kDefaultUsers[0].password,
                                        system_salt_, &passkey);
  UsernamePasskey up(kDefaultUsers[0].username, passkey);

  EXPECT_FALSE(mount.Init());
  ASSERT_FALSE(mount.TestCredentials(up));
}

TEST_F(MountTest, GoodDecryptTest) {
  // create a Mount instance that points to a good shadow root, test that it
  // properly authenticates against the first key.
  Mount mount;
  mount.set_shadow_root(kImageDir);
  mount.set_skel_source(kSkelDir);
  mount.set_use_tpm(false);
  mount.set_fallback_to_scrypt(true);

  cryptohome::SecureBlob passkey;
  cryptohome::Crypto::PasswordToPasskey(kDefaultUsers[1].password,
                                        system_salt_, &passkey);
  UsernamePasskey up(kDefaultUsers[1].username, passkey);

  EXPECT_TRUE(mount.Init());
  ASSERT_TRUE(mount.TestCredentials(up));
}

TEST_F(MountTest, TestCredsDoesNotReSave) {
  // create a Mount instance that points to a good shadow root, test that it
  // properly authenticates against the first key.
  Mount mount;
  mount.set_shadow_root(kImageDir);
  mount.set_skel_source(kSkelDir);
  mount.set_use_tpm(false);
  mount.set_fallback_to_scrypt(true);

  cryptohome::SecureBlob passkey;
  cryptohome::Crypto::PasswordToPasskey(kDefaultUsers[2].password,
                                        system_salt_, &passkey);
  UsernamePasskey up(kDefaultUsers[2].username, passkey);

  EXPECT_TRUE(mount.Init());

  // Make sure the keyset is not scrypt wrapped
  std::string key_path = mount.GetUserKeyFile(up);
  cryptohome::SerializedVaultKeyset serialized;
  ASSERT_TRUE(LoadSerializedKeyset(key_path, &serialized));
  ASSERT_EQ(0, (serialized.flags() &
                cryptohome::SerializedVaultKeyset::SCRYPT_WRAPPED));

  ASSERT_TRUE(mount.TestCredentials(up));

  // Make sure the keyset is still not scrypt wrapped
  cryptohome::SerializedVaultKeyset serialized2;
  ASSERT_TRUE(LoadSerializedKeyset(key_path, &serialized2));
  ASSERT_EQ(0, (serialized2.flags() &
                cryptohome::SerializedVaultKeyset::SCRYPT_WRAPPED));
}

TEST_F(MountTest, CurrentCredentialsTest) {
  // create a Mount instance that points to a good shadow root, test that it
  // properly authenticates against the first key
  Mount mount;
  mount.set_shadow_root(kImageDir);
  mount.set_skel_source(kSkelDir);

  cryptohome::SecureBlob passkey;
  cryptohome::Crypto::PasswordToPasskey(kDefaultUsers[3].password,
                                        system_salt_, &passkey);
  UsernamePasskey up(kDefaultUsers[3].username, passkey);

  EXPECT_TRUE(mount.Init());

  MockUserSession user_session;
  Crypto crypto;
  user_session.Init(&crypto, SecureBlob());
  user_session.SetUser(up);
  mount.set_current_user(&user_session);

  EXPECT_CALL(user_session, CheckUser(_))
      .WillOnce(Return(true));
  EXPECT_CALL(user_session, Verify(_))
      .WillOnce(Return(true));

  ASSERT_TRUE(mount.TestCredentials(up));
}

TEST_F(MountTest, BadDecryptTest) {
  // create a Mount instance that points to a good shadow root, test that it
  // properly denies access with a bad passkey
  Mount mount;
  mount.set_shadow_root(kImageDir);
  mount.set_skel_source(kSkelDir);

  cryptohome::SecureBlob passkey;
  cryptohome::Crypto::PasswordToPasskey("bogus", system_salt_, &passkey);
  UsernamePasskey up(kDefaultUsers[4].username, passkey);

  EXPECT_TRUE(mount.Init());
  ASSERT_FALSE(mount.TestCredentials(up));
}

TEST_F(MountTest, CreateCryptohomeTest) {
  // creates a cryptohome and tests credentials
  Mount mount;
  mount.set_shadow_root(kImageDir);
  mount.set_skel_source(kSkelDir);
  mount.set_set_vault_ownership(false);

  // Test user at index 5 was not created by the test data
  cryptohome::SecureBlob passkey;
  cryptohome::Crypto::PasswordToPasskey(kDefaultUsers[5].password,
                                        system_salt_, &passkey);
  UsernamePasskey up(kDefaultUsers[5].username, passkey);

  EXPECT_TRUE(mount.Init());
  bool created;
  ASSERT_TRUE(mount.EnsureCryptohome(up, &created));
  ASSERT_TRUE(created);

  FilePath image_dir(kImageDir);
  FilePath user_path = image_dir.Append(up.GetObfuscatedUsername(system_salt_));
  FilePath key_path = user_path.Append("master.0");
  FilePath vault_path = user_path.Append("vault");
  FilePath skel_testfile_path = user_path.Append("sub_path/.testfile");

  ASSERT_TRUE(file_util::PathExists(key_path));
  ASSERT_TRUE(file_util::PathExists(vault_path));
  ASSERT_TRUE(mount.TestCredentials(up));
}

TEST_F(MountTest, GoodReDecryptTest) {
  // create a Mount instance that points to a good shadow root, test that it
  // properly re-authenticates against the first key
  Mount mount;
  mount.set_shadow_root(kImageDir);
  mount.set_skel_source(kSkelDir);

  cryptohome::SecureBlob passkey;
  cryptohome::Crypto::PasswordToPasskey(kDefaultUsers[6].password,
                                        system_salt_, &passkey);
  UsernamePasskey up(kDefaultUsers[6].username, passkey);

  EXPECT_TRUE(mount.Init());

  // Make sure the keyset is not scrypt wrapped
  std::string key_path = mount.GetUserKeyFile(up);
  cryptohome::SerializedVaultKeyset serialized;
  ASSERT_TRUE(LoadSerializedKeyset(key_path, &serialized));
  ASSERT_EQ(0, (serialized.flags() &
                cryptohome::SerializedVaultKeyset::SCRYPT_WRAPPED));

  // Call UnwrapVaultKeyset first, allowing migration (the test data is not
  // scrypt nor TPM wrapped) to a scrypt-wrapped keyset
  VaultKeyset vault_keyset;
  Mount::MountError error;
  ASSERT_TRUE(mount.UnwrapVaultKeyset(up, true, &vault_keyset, &error));

  // Make sure the keyset is now scrypt wrapped
  cryptohome::SerializedVaultKeyset serialized2;
  ASSERT_TRUE(LoadSerializedKeyset(key_path, &serialized2));
  ASSERT_EQ(SerializedVaultKeyset::SCRYPT_WRAPPED,
            (serialized2.flags() &
             cryptohome::SerializedVaultKeyset::SCRYPT_WRAPPED));

  ASSERT_TRUE(mount.TestCredentials(up));
}

TEST_F(MountTest, MigrateTest) {
  // create a Mount instance that points to a good shadow root, test that it
  // will migrate an old style key
  Mount mount;
  mount.set_shadow_root(kImageDir);
  mount.set_skel_source(kSkelDir);

  // Test user at index 7 was created using the old style
  cryptohome::SecureBlob passkey;
  cryptohome::Crypto::PasswordToPasskey(kDefaultUsers[7].password,
                                        system_salt_, &passkey);
  UsernamePasskey up(kDefaultUsers[7].username, passkey);

  EXPECT_TRUE(mount.Init());

  // Make sure the keyset is not scrypt wrapped
  std::string salt_path = mount.GetUserSaltFile(up);
  ASSERT_TRUE(file_util::PathExists(FilePath(salt_path)));

  // Call UnwrapVaultKeyset first, allowing migration (the test data is not
  // scrypt nor TPM wrapped) to a scrypt-wrapped keyset
  VaultKeyset vault_keyset;
  Mount::MountError error;
  ASSERT_TRUE(mount.UnwrapVaultKeyset(up, true, &vault_keyset, &error));

  // Make sure the salt path no longer exists
  ASSERT_FALSE(file_util::PathExists(FilePath(salt_path)));

  // Make sure the keyset is now scrypt wrapped
  std::string key_path = mount.GetUserKeyFile(up);
  cryptohome::SerializedVaultKeyset serialized;
  ASSERT_TRUE(LoadSerializedKeyset(key_path, &serialized));
  ASSERT_EQ(SerializedVaultKeyset::SCRYPT_WRAPPED,
            (serialized.flags() &
             cryptohome::SerializedVaultKeyset::SCRYPT_WRAPPED));

  ASSERT_TRUE(mount.TestCredentials(up));
}

TEST_F(MountTest, SystemSaltTest) {
  // checks that cryptohome reads the system salt
  Mount mount;
  mount.set_shadow_root(kImageDir);
  mount.set_skel_source(kSkelDir);

  EXPECT_TRUE(mount.Init());
  chromeos::Blob system_salt;
  mount.GetSystemSalt(&system_salt);
  ASSERT_TRUE((system_salt.size() == system_salt_.size()));
  ASSERT_EQ(0, memcmp(&system_salt[0], &system_salt_[0],
                         system_salt.size()));
}

} // namespace cryptohome
