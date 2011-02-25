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
const char kAltImageDir[] = "alt_test_image_dir";
const char kAltHomeDir[] = "alt_test_home_dir";

class MountTest : public ::testing::Test {
 public:
  MountTest() { }
  virtual ~MountTest() { }

  void SetUp() {
    LoadSystemSalt(kImageDir);
  }

  void LoadSystemSalt(const char* str_image_path) {
    FilePath image_dir(str_image_path);
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
  ASSERT_TRUE(mount.EnsureCryptohome(up, Mount::MountArgs(), &created));
  ASSERT_TRUE(created);

  FilePath image_dir(kImageDir);
  FilePath user_path = image_dir.Append(up.GetObfuscatedUsername(system_salt_));
  FilePath key_path = user_path.Append("master.0");
  FilePath vault_path = user_path.Append("vault");

  ASSERT_TRUE(file_util::PathExists(key_path));
  ASSERT_TRUE(file_util::PathExists(vault_path));
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

  // Call DecryptVaultKeyset first, allowing migration (the test data is not
  // scrypt nor TPM wrapped) to a scrypt-wrapped keyset
  VaultKeyset vault_keyset;
  Mount::MountError error;
  ASSERT_TRUE(mount.DecryptVaultKeyset(up, true, &vault_keyset, &serialized,
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

  // Call DecryptVaultKeyset first, allowing migration (the test data is not
  // scrypt nor TPM wrapped) to a scrypt-wrapped keyset
  VaultKeyset vault_keyset;
  SerializedVaultKeyset serialized;
  Mount::MountError error;
  ASSERT_TRUE(mount.DecryptVaultKeyset(up, true, &vault_keyset, &serialized,
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
  ASSERT_TRUE(mount.DecryptVaultKeyset(up, true, &vault_keyset, &serialized,
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

  cryptohome::SecureBlob passkey;
  cryptohome::Crypto::PasswordToPasskey(kDefaultUsers[10].password,
                                        system_salt_, &passkey);
  UsernamePasskey up(kDefaultUsers[10].username, passkey);

  EXPECT_CALL(platform, Mount(_, _, _, _))
      .WillRepeatedly(Return(true));

  Mount::MountError error;
  ASSERT_TRUE(mount.MountCryptohome(up, Mount::MountArgs(), &error));

  FilePath image_dir(kImageDir);
  FilePath user_path = image_dir.Append(up.GetObfuscatedUsername(system_salt_));
  FilePath vault_path = user_path.Append("vault");
  FilePath subdir_path = vault_path.Append(kCacheDir);
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

  cryptohome::SecureBlob passkey;
  cryptohome::Crypto::PasswordToPasskey(kDefaultUsers[11].password,
                                        system_salt_, &passkey);
  UsernamePasskey up(kDefaultUsers[11].username, passkey);

  VaultKeyset vault_keyset;
  SerializedVaultKeyset serialized;
  Mount::MountError error;
  ASSERT_TRUE(mount.DecryptVaultKeyset(up, true, &vault_keyset, &serialized,
                                       &error));

  EXPECT_CALL(platform, Mount(_, _, _, _))
      .WillOnce(Return(true));

  ASSERT_TRUE(mount.MountCryptohome(up, Mount::MountArgs(), &error));

  // Make sure the keyset now has only one tracked directory, "Cache"
  SerializedVaultKeyset new_serialized;
  ASSERT_TRUE(mount.DecryptVaultKeyset(up, true, &vault_keyset, &new_serialized,
                                       &error));

  FilePath image_dir(kImageDir);
  FilePath user_path = image_dir.Append(up.GetObfuscatedUsername(system_salt_));
  FilePath vault_path = user_path.Append("vault");
  FilePath subdir_path = vault_path.Append(kCacheDir);
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

  Mount::MountArgs mount_args;
  mount_args.create_if_missing = false;
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

  FilePath subdir_path = vault_path.Append(kCacheDir);
  ASSERT_TRUE(file_util::PathExists(subdir_path));
}

TEST_F(MountTest, RemoveSubdirectories) {
  // checks that cryptohome can delete tracked subdirectories
  LoadSystemSalt(kAltImageDir);
  Mount mount;
  NiceMock<MockTpm> tpm;
  mount.get_crypto()->set_tpm(&tpm);
  mount.set_shadow_root(kAltImageDir);
  mount.set_skel_source(kSkelDir);
  mount.set_use_tpm(false);

  NiceMock<MockPlatform> platform;
  mount.set_platform(&platform);

  EXPECT_TRUE(mount.Init());

  cryptohome::SecureBlob passkey;
  cryptohome::Crypto::PasswordToPasskey(kAlternateUsers[0].password,
                                        system_salt_, &passkey);
  UsernamePasskey up(kAlternateUsers[0].username, passkey);

  EXPECT_CALL(platform, Mount(_, _, _, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(platform, Unmount(_, _, _))
      .WillRepeatedly(Return(true));

  Mount::MountError error;
  EXPECT_TRUE(mount.MountCryptohome(up, Mount::MountArgs(), &error));

  FilePath image_dir(kAltImageDir);
  FilePath user_path = image_dir.Append(up.GetObfuscatedUsername(system_salt_));
  FilePath vault_path = user_path.Append("vault");
  FilePath subdir_path = vault_path.Append(kCacheDir);
  ASSERT_TRUE(file_util::PathExists(subdir_path));

  NiceMock<MockPlatform> platform_mounted;
  mount.set_platform(&platform_mounted);
  // Make sure the directory is not deleted if the vault is mounted
  EXPECT_CALL(platform_mounted, IsDirectoryMounted(_))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_mounted, IsDirectoryMountedWith(_, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_mounted, Mount(_, _, _, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_mounted, Unmount(_, _, _))
      .WillRepeatedly(Return(true));

  mount.CleanUnmountedTrackedSubdirectories();

  ASSERT_TRUE(file_util::PathExists(subdir_path));

  mount.UnmountCryptohome();

  NiceMock<MockPlatform> platform_unmounted;
  mount.set_platform(&platform_unmounted);
  // Make sure the directory is not deleted if the vault is mounted
  EXPECT_CALL(platform_unmounted, IsDirectoryMounted(_))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(platform_unmounted, IsDirectoryMountedWith(_, _))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(platform_unmounted, Mount(_, _, _, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_unmounted, Unmount(_, _, _))
      .WillRepeatedly(Return(true));

  mount.CleanUnmountedTrackedSubdirectories();

  ASSERT_FALSE(file_util::PathExists(subdir_path));
}

TEST_F(MountTest, MigrationOfTrackedDirs) {
  // Checks that old cryptohomes (without pass-through tracked
  // directories) migrate when Mount()ed.
  LoadSystemSalt(kAltImageDir);
  Mount mount;
  NiceMock<MockTpm> tpm;
  mount.get_crypto()->set_tpm(&tpm);
  mount.set_shadow_root(kAltImageDir);
  mount.set_skel_source(kSkelDir);
  mount.set_use_tpm(false);

  NiceMock<MockPlatform> platform;
  mount.set_platform(&platform);

  EXPECT_TRUE(mount.Init());

  cryptohome::SecureBlob passkey;
  cryptohome::Crypto::PasswordToPasskey(kAlternateUsers[1].password,
                                        system_salt_, &passkey);
  UsernamePasskey up(kAlternateUsers[1].username, passkey);

  // As we don't have real mount in the test, immagine its output (home)
  // directory.
  FilePath home_dir(kAltHomeDir);
  file_util::CreateDirectory(home_dir);
  mount.set_home_dir(home_dir.value());

  // Pretend that mounted cryptohome already had non-pass-through
  // subdirs "Cache" and "Downloads".
  FilePath cache_dir(home_dir.Append(kCacheDir));
  FilePath downloads_dir(home_dir.Append(kDownloadsDir));
  file_util::CreateDirectory(cache_dir);
  file_util::CreateDirectory(downloads_dir);

  // And they are not empty.
  const string contents = "Hello world!!!";
  file_util::WriteFile(cache_dir.Append("cached_file"),
                       contents.c_str(), contents.length());
  file_util::WriteFile(downloads_dir.Append("downloaded_file"),
                       contents.c_str(), contents.length());

  // Even have subdirectories.
  FilePath cache_subdir(cache_dir.Append("cache_subdir"));
  FilePath downloads_subdir(downloads_dir.Append("downloads_subdir"));
  file_util::CreateDirectory(cache_subdir);
  file_util::CreateDirectory(downloads_subdir);
  file_util::WriteFile(cache_subdir.Append("cached_file"),
                       contents.c_str(), contents.length());
  file_util::WriteFile(downloads_subdir.Append("downloaded_file"),
                       contents.c_str(), contents.length());

  // Now Mount().
  EXPECT_CALL(platform, Mount(_, _, _, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(platform, Unmount(_, _, _))
      .WillRepeatedly(Return(true));
  Mount::MountError error;
  EXPECT_TRUE(mount.MountCryptohome(up, Mount::MountArgs(), &error));

  // Check that vault path now have pass-through version of tracked dirs.
  FilePath image_dir(kAltImageDir);
  FilePath user_path = image_dir.Append(up.GetObfuscatedUsername(system_salt_));
  FilePath vault_path = user_path.Append("vault");
  ASSERT_TRUE(file_util::PathExists(vault_path.Append(kCacheDir)));
  ASSERT_TRUE(file_util::PathExists(vault_path.Append(kDownloadsDir)));

  // Check that Cache is clear (because it does not need migration) so
  // it should not appear in a home dir.
  EXPECT_FALSE(file_util::PathExists(cache_dir));

  // Check that Downloads is completely migrated.
  string tested;
  EXPECT_TRUE(file_util::PathExists(downloads_dir));
  EXPECT_TRUE(file_util::ReadFileToString(
      downloads_dir.Append("downloaded_file"), &tested));
  EXPECT_EQ(contents, tested);
  EXPECT_TRUE(file_util::PathExists(downloads_subdir));
  tested.clear();
  EXPECT_TRUE(file_util::ReadFileToString(
      downloads_subdir.Append("downloaded_file"), &tested));
  EXPECT_EQ(contents, tested);

  // Check that we did not leave any litter.
  file_util::Delete(downloads_dir, true);
  EXPECT_TRUE(file_util::IsDirectoryEmpty(home_dir));
}

} // namespace cryptohome
