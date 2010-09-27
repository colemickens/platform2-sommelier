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
#include "mock_platform.h"
#include "mock_tpm.h"
#include "mock_user_session.h"
#include "vault_keyset.h"
#include "vault_keyset.pb.h"

namespace cryptohome {
using std::string;
using ::testing::Return;
using ::testing::_;
using ::testing::NiceMock;

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

  void GetKeysetBlob(const SerializedVaultKeyset& serialized,
                     SecureBlob* blob) {
    SecureBlob local_wrapped_keyset(serialized.wrapped_keyset().length());
    serialized.wrapped_keyset().copy(
        static_cast<char*>(local_wrapped_keyset.data()),
        serialized.wrapped_keyset().length(), 0);
    blob->swap(local_wrapped_keyset);
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
  NiceMock<MockTpm> tpm;
  mount.get_crypto()->set_tpm(&tpm);
  mount.set_shadow_root("/dev/null");
  mount.set_skel_source(kSkelDir);
  mount.set_use_tpm(false);

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
  NiceMock<MockTpm> tpm;
  mount.get_crypto()->set_tpm(&tpm);
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
  NiceMock<MockTpm> tpm;
  mount.get_crypto()->set_tpm(&tpm);
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
  NiceMock<MockTpm> tpm;
  mount.get_crypto()->set_tpm(&tpm);
  mount.set_shadow_root(kImageDir);
  mount.set_skel_source(kSkelDir);
  mount.set_use_tpm(false);

  cryptohome::SecureBlob passkey;
  cryptohome::Crypto::PasswordToPasskey(kDefaultUsers[3].password,
                                        system_salt_, &passkey);
  UsernamePasskey up(kDefaultUsers[3].username, passkey);

  EXPECT_TRUE(mount.Init());

  NiceMock<MockUserSession> user_session;
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
  NiceMock<MockTpm> tpm;
  mount.get_crypto()->set_tpm(&tpm);
  mount.set_shadow_root(kImageDir);
  mount.set_skel_source(kSkelDir);
  mount.set_use_tpm(false);

  cryptohome::SecureBlob passkey;
  cryptohome::Crypto::PasswordToPasskey("bogus", system_salt_, &passkey);
  UsernamePasskey up(kDefaultUsers[4].username, passkey);

  EXPECT_TRUE(mount.Init());
  ASSERT_FALSE(mount.TestCredentials(up));
}

TEST_F(MountTest, CreateCryptohomeTest) {
  // creates a cryptohome and tests credentials
  Mount mount;
  NiceMock<MockTpm> tpm;
  mount.get_crypto()->set_tpm(&tpm);
  mount.set_shadow_root(kImageDir);
  mount.set_skel_source(kSkelDir);
  mount.set_use_tpm(false);
  mount.set_set_vault_ownership(false);

  // Test user at index 5 was not created by the test data
  cryptohome::SecureBlob passkey;
  cryptohome::Crypto::PasswordToPasskey(kDefaultUsers[5].password,
                                        system_salt_, &passkey);
  UsernamePasskey up(kDefaultUsers[5].username, passkey);

  EXPECT_TRUE(mount.Init());
  bool created;
  const char* subdirs[] = {"subdir", NULL};
  Mount::MountArgs mount_args;
  mount_args.AssignSubdirsNullTerminatedList(subdirs);
  ASSERT_TRUE(mount.EnsureCryptohome(up, mount_args, &created));
  ASSERT_TRUE(created);

  FilePath image_dir(kImageDir);
  FilePath user_path = image_dir.Append(up.GetObfuscatedUsername(system_salt_));
  FilePath key_path = user_path.Append("master.0");
  FilePath vault_path = user_path.Append("vault");
  FilePath skel_testfile_path = user_path.Append("sub_path/.testfile");
  FilePath subdir_path = vault_path.Append("subdir");

  ASSERT_TRUE(file_util::PathExists(key_path));
  ASSERT_TRUE(file_util::PathExists(vault_path));
  ASSERT_TRUE(file_util::PathExists(subdir_path));
  ASSERT_TRUE(mount.TestCredentials(up));
}

TEST_F(MountTest, GoodReDecryptTest) {
  // create a Mount instance that points to a good shadow root, test that it
  // properly re-authenticates against the first key
  Mount mount;
  NiceMock<MockTpm> tpm;
  mount.get_crypto()->set_tpm(&tpm);
  mount.set_use_tpm(false);
  mount.set_shadow_root(kImageDir);
  mount.set_skel_source(kSkelDir);
  mount.set_use_tpm(false);

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
  ASSERT_TRUE(mount.UnwrapVaultKeyset(up, true, &vault_keyset, &serialized,
                                      &error));

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
  NiceMock<MockTpm> tpm;
  mount.get_crypto()->set_tpm(&tpm);
  mount.set_shadow_root(kImageDir);
  mount.set_skel_source(kSkelDir);
  mount.set_use_tpm(false);

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
  SerializedVaultKeyset serialized;
  Mount::MountError error;
  ASSERT_TRUE(mount.UnwrapVaultKeyset(up, true, &vault_keyset, &serialized,
                                      &error));

  // Make sure the salt path no longer exists
  ASSERT_FALSE(file_util::PathExists(FilePath(salt_path)));

  // Make sure the keyset is now scrypt wrapped
  std::string key_path = mount.GetUserKeyFile(up);
  ASSERT_TRUE(LoadSerializedKeyset(key_path, &serialized));
  ASSERT_EQ(SerializedVaultKeyset::SCRYPT_WRAPPED,
            (serialized.flags() &
             cryptohome::SerializedVaultKeyset::SCRYPT_WRAPPED));

  ASSERT_TRUE(mount.TestCredentials(up));
}

TEST_F(MountTest, SystemSaltTest) {
  // checks that cryptohome reads the system salt
  Mount mount;
  NiceMock<MockTpm> tpm;
  mount.get_crypto()->set_tpm(&tpm);
  mount.set_shadow_root(kImageDir);
  mount.set_skel_source(kSkelDir);
  mount.set_use_tpm(false);

  EXPECT_TRUE(mount.Init());
  chromeos::Blob system_salt;
  mount.GetSystemSalt(&system_salt);
  ASSERT_TRUE((system_salt.size() == system_salt_.size()));
  ASSERT_EQ(0, memcmp(&system_salt[0], &system_salt_[0],
                         system_salt.size()));
}

TEST_F(MountTest, ChangeTrackedDirs) {
  // create a Mount instance that points to a good shadow root, test that it
  // will re-save the vault keyset on tracked dirs change
  Mount mount;
  NiceMock<MockTpm> tpm;
  mount.get_crypto()->set_tpm(&tpm);
  mount.set_shadow_root(kImageDir);
  mount.set_skel_source(kSkelDir);
  mount.set_use_tpm(false);

  // Test user at index 9 has a tracked dir "DIR0"
  cryptohome::SecureBlob passkey;
  cryptohome::Crypto::PasswordToPasskey(kDefaultUsers[9].password,
                                        system_salt_, &passkey);
  UsernamePasskey up(kDefaultUsers[9].username, passkey);

  EXPECT_TRUE(mount.Init());

  // Make sure the keyset has only one tracked directory, "DIR0"
  VaultKeyset vault_keyset;
  SerializedVaultKeyset serialized;
  Mount::MountError error;
  ASSERT_TRUE(mount.UnwrapVaultKeyset(up, true, &vault_keyset, &serialized,
                                      &error));

  ASSERT_EQ(1, serialized.tracked_subdirectories_size());
  ASSERT_EQ(0, serialized.tracked_subdirectories(0).compare("DIR0"));

  // Make sure the tracked dirs change.  serialized starts with DIR0
  std::vector<std::string> new_dirs;
  new_dirs.push_back("DIR0");
  ASSERT_FALSE(mount.ReplaceTrackedSubdirectories(new_dirs, &serialized));
  // serialized now has "DIR0"
  ASSERT_EQ(1, serialized.tracked_subdirectories_size());

  new_dirs.clear();
  new_dirs.push_back("DIR1");
  ASSERT_TRUE(mount.ReplaceTrackedSubdirectories(new_dirs, &serialized));
  // serialized now has "DIR1"
  ASSERT_EQ(1, serialized.tracked_subdirectories_size());

  new_dirs.clear();
  new_dirs.push_back("DIR1");
  new_dirs.push_back("DIR0");
  ASSERT_TRUE(mount.ReplaceTrackedSubdirectories(new_dirs, &serialized));
  // serialized now has "DIR1", "DIR0"
  ASSERT_EQ(2, serialized.tracked_subdirectories_size());

  new_dirs.clear();
  new_dirs.push_back("DIR0");
  new_dirs.push_back("DIR1");
  ASSERT_FALSE(mount.ReplaceTrackedSubdirectories(new_dirs, &serialized));
  // serialized now has "DIR1", "DIR0"
  ASSERT_EQ(2, serialized.tracked_subdirectories_size());

  new_dirs.clear();
  new_dirs.push_back("DIR0");
  ASSERT_TRUE(mount.ReplaceTrackedSubdirectories(new_dirs, &serialized));
  // serialized now has "DIR0"
  ASSERT_EQ(1, serialized.tracked_subdirectories_size());

  new_dirs.clear();
  ASSERT_TRUE(mount.ReplaceTrackedSubdirectories(new_dirs, &serialized));
  // serialized now has nothing
  ASSERT_EQ(0, serialized.tracked_subdirectories_size());
}

TEST_F(MountTest, MountCryptohome) {
  // checks that cryptohome tries to mount successfully, and tests that the
  // tracked directories are created/replaced as expected
  Mount mount;
  NiceMock<MockTpm> tpm;
  mount.get_crypto()->set_tpm(&tpm);
  mount.set_shadow_root(kImageDir);
  mount.set_skel_source(kSkelDir);
  mount.set_use_tpm(false);

  NiceMock<MockPlatform> platform;
  mount.set_platform(&platform);

  EXPECT_TRUE(mount.Init());

  // Test user at index 9 has a tracked dir "DIR0"
  cryptohome::SecureBlob passkey;
  cryptohome::Crypto::PasswordToPasskey(kDefaultUsers[10].password,
                                        system_salt_, &passkey);
  UsernamePasskey up(kDefaultUsers[10].username, passkey);

  EXPECT_CALL(platform, Mount(_, _, _, _))
      .WillRepeatedly(Return(true));

  const char* new_dirs[] = {"DIR1", NULL};
  Mount::MountArgs mount_args;
  mount_args.AssignSubdirsNullTerminatedList(new_dirs);
  Mount::MountError error;
  ASSERT_TRUE(mount.MountCryptohome(up, mount_args, &error));

  // Make sure the keyset now has only one tracked directory, "DIR0"
  VaultKeyset vault_keyset;
  SerializedVaultKeyset serialized;
  ASSERT_TRUE(mount.UnwrapVaultKeyset(up, true, &vault_keyset, &serialized,
                                      &error));

  ASSERT_EQ(1, serialized.tracked_subdirectories_size());
  ASSERT_EQ(0, serialized.tracked_subdirectories(0).compare("DIR0"));

  mount_args.replace_tracked_subdirectories = true;
  ASSERT_TRUE(mount.MountCryptohome(up, mount_args, &error));

  // Make sure the keyset now has only one tracked directory, "DIR1"
  ASSERT_TRUE(mount.UnwrapVaultKeyset(up, true, &vault_keyset, &serialized,
                                      &error));

  ASSERT_EQ(1, serialized.tracked_subdirectories_size());
  ASSERT_EQ(0, serialized.tracked_subdirectories(0).compare("DIR1"));

  FilePath image_dir(kImageDir);
  FilePath user_path = image_dir.Append(up.GetObfuscatedUsername(system_salt_));
  FilePath vault_path = user_path.Append("vault");
  FilePath subdir_path = vault_path.Append("DIR1");
  ASSERT_TRUE(file_util::PathExists(subdir_path));
}

TEST_F(MountTest, MountCryptohomeNoChange) {
  // checks that cryptohome doesn't by default re-save the cryptohome when an
  // identical list of tracked directories is passed in
  Mount mount;
  NiceMock<MockTpm> tpm;
  mount.get_crypto()->set_tpm(&tpm);
  mount.set_shadow_root(kImageDir);
  mount.set_skel_source(kSkelDir);
  mount.set_use_tpm(false);

  NiceMock<MockPlatform> platform;
  mount.set_platform(&platform);

  EXPECT_TRUE(mount.Init());

  // Test user at index 11 has a tracked dir "DIR0"
  cryptohome::SecureBlob passkey;
  cryptohome::Crypto::PasswordToPasskey(kDefaultUsers[11].password,
                                        system_salt_, &passkey);
  UsernamePasskey up(kDefaultUsers[11].username, passkey);

  VaultKeyset vault_keyset;
  SerializedVaultKeyset serialized;
  Mount::MountError error;
  ASSERT_TRUE(mount.UnwrapVaultKeyset(up, true, &vault_keyset, &serialized,
                                      &error));

  EXPECT_CALL(platform, Mount(_, _, _, _))
      .WillOnce(Return(true));

  const char* new_dirs[] = {"DIR0", NULL};
  Mount::MountArgs mount_args;
  mount_args.replace_tracked_subdirectories = true;
  mount_args.AssignSubdirsNullTerminatedList(new_dirs);
  ASSERT_TRUE(mount.MountCryptohome(up, mount_args, &error));

  // Make sure the keyset now has only one tracked directory, "DIR0"
  SerializedVaultKeyset new_serialized;
  ASSERT_TRUE(mount.UnwrapVaultKeyset(up, true, &vault_keyset, &new_serialized,
                                      &error));

  ASSERT_EQ(1, new_serialized.tracked_subdirectories_size());
  ASSERT_EQ(0, new_serialized.tracked_subdirectories(0).compare("DIR0"));

  FilePath image_dir(kImageDir);
  FilePath user_path = image_dir.Append(up.GetObfuscatedUsername(system_salt_));
  FilePath vault_path = user_path.Append("vault");
  FilePath subdir_path = vault_path.Append("DIR0");
  ASSERT_TRUE(file_util::PathExists(subdir_path));

  SecureBlob lhs;
  GetKeysetBlob(serialized, &lhs);
  SecureBlob rhs;
  GetKeysetBlob(new_serialized, &rhs);
  ASSERT_EQ(lhs.size(), rhs.size());
  ASSERT_EQ(0, memcmp(lhs.data(), rhs.data(), lhs.size()));
}

TEST_F(MountTest, MountCryptohomeNoCreate) {
  // checks that doesn't create the cryptohome for the user on Mount without
  // being told to do so.
  Mount mount;
  NiceMock<MockTpm> tpm;
  mount.get_crypto()->set_tpm(&tpm);
  mount.set_shadow_root(kImageDir);
  mount.set_skel_source(kSkelDir);
  mount.set_use_tpm(false);

  NiceMock<MockPlatform> platform;
  mount.set_platform(&platform);

  EXPECT_TRUE(mount.Init());

  // Test user at index 12 hasn't been created
  cryptohome::SecureBlob passkey;
  cryptohome::Crypto::PasswordToPasskey(kDefaultUsers[12].password,
                                        system_salt_, &passkey);
  UsernamePasskey up(kDefaultUsers[12].username, passkey);

  EXPECT_CALL(platform, Mount(_, _, _, _))
      .WillOnce(Return(true));

  const char* new_dirs[] = {"DIR0", NULL};
  Mount::MountArgs mount_args;
  mount_args.create_if_missing = false;
  mount_args.AssignSubdirsNullTerminatedList(new_dirs);
  Mount::MountError error;
  ASSERT_FALSE(mount.MountCryptohome(up, mount_args, &error));
  ASSERT_EQ(Mount::MOUNT_ERROR_USER_DOES_NOT_EXIST, error);

  FilePath image_dir(kImageDir);
  FilePath user_path = image_dir.Append(up.GetObfuscatedUsername(system_salt_));
  FilePath vault_path = user_path.Append("vault");
  ASSERT_FALSE(file_util::PathExists(vault_path));

  mount_args.create_if_missing = true;
  ASSERT_TRUE(mount.MountCryptohome(up, mount_args, &error));
  ASSERT_TRUE(file_util::PathExists(vault_path));

  VaultKeyset vault_keyset;
  SerializedVaultKeyset serialized;
  // Make sure the keyset now has only one tracked directory, "DIR0"
  ASSERT_TRUE(mount.UnwrapVaultKeyset(up, true, &vault_keyset, &serialized,
                                      &error));

  ASSERT_EQ(1, serialized.tracked_subdirectories_size());
  ASSERT_EQ(0, serialized.tracked_subdirectories(0).compare("DIR0"));

  FilePath subdir_path = vault_path.Append("DIR0");
  ASSERT_TRUE(file_util::PathExists(subdir_path));
}

} // namespace cryptohome
