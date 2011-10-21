// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
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
#include <base/time.h>
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
using ::testing::InSequence;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::_;

const char kImageDir[] = "test_image_dir";
const char kSkelDir[] = "test_image_dir/skel";
const char kHomeDir[] = "alt_test_home_dir";
const char kUserDir[] = "user";

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
  FilePath vault_path = user_path.Append(kVaultDir);

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
  ASSERT_EQ(0, chromeos::SafeMemcmp(&system_salt[0], &system_salt_[0],
                                    system_salt.size()));
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
  EXPECT_CALL(platform, Bind(_, _))
      .WillRepeatedly(Return(true));

  Mount::MountError error;
  ASSERT_TRUE(mount.MountCryptohome(up, Mount::MountArgs(), &error));

  FilePath image_dir(kImageDir);
  FilePath user_path = image_dir.Append(up.GetObfuscatedUsername(system_salt_));
  FilePath vault_path = user_path.Append(kVaultDir);
  FilePath vault_user_path = vault_path.Append(kUserDir);
  FilePath subdir_path = vault_user_path.Append(kCacheDir);
  ASSERT_TRUE(file_util::PathExists(subdir_path));
}

TEST_F(MountTest, MountCryptohomeNoChange) {
  // checks that cryptohome doesn't by default re-save the cryptohome when mount
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
  EXPECT_CALL(platform, Bind(_, _))
      .Times(3)
      .WillRepeatedly(Return(true));

  ASSERT_TRUE(mount.MountCryptohome(up, Mount::MountArgs(), &error));

  SerializedVaultKeyset new_serialized;
  ASSERT_TRUE(mount.DecryptVaultKeyset(up, true, &vault_keyset, &new_serialized,
                                       &error));

  SecureBlob lhs;
  GetKeysetBlob(serialized, &lhs);
  SecureBlob rhs;
  GetKeysetBlob(new_serialized, &rhs);
  ASSERT_EQ(lhs.size(), rhs.size());
  ASSERT_EQ(0, chromeos::SafeMemcmp(lhs.data(), rhs.data(), lhs.size()));
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
  EXPECT_CALL(platform, Bind(_, _))
      .Times(3)
      .WillRepeatedly(Return(true));

  Mount::MountArgs mount_args;
  mount_args.create_if_missing = false;
  Mount::MountError error;
  ASSERT_FALSE(mount.MountCryptohome(up, mount_args, &error));
  ASSERT_EQ(Mount::MOUNT_ERROR_USER_DOES_NOT_EXIST, error);

  FilePath image_dir(kImageDir);
  FilePath user_path = image_dir.Append(up.GetObfuscatedUsername(system_salt_));
  FilePath vault_path = user_path.Append(kVaultDir);
  ASSERT_FALSE(file_util::PathExists(vault_path));

  mount_args.create_if_missing = true;
  ASSERT_TRUE(mount.MountCryptohome(up, mount_args, &error));
  ASSERT_TRUE(file_util::PathExists(vault_path));

  FilePath vault_user_path = vault_path.Append(kUserDir);
  FilePath subdir_path = vault_user_path.Append(kCacheDir);
  ASSERT_TRUE(file_util::PathExists(subdir_path));
}

TEST_F(MountTest, RemoveSubdirectories) {
  // checks that cryptohome can delete tracked subdirectories
  LoadSystemSalt(kImageDir);
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
  cryptohome::Crypto::PasswordToPasskey(kDefaultUsers[13].password,
                                        system_salt_, &passkey);
  UsernamePasskey up(kDefaultUsers[13].username, passkey);

  EXPECT_CALL(platform, Mount(_, _, _, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(platform, Bind(_, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(platform, Unmount(_, _, _))
      .WillRepeatedly(Return(true));

  Mount::MountError error;
  EXPECT_TRUE(mount.MountCryptohome(up, Mount::MountArgs(), &error));

  FilePath image_dir(kImageDir);
  FilePath user_path = image_dir.Append(up.GetObfuscatedUsername(system_salt_));
  FilePath vault_path = user_path.Append(kVaultDir);
  FilePath vault_user_path = vault_path.Append(kUserDir);
  FilePath subdir_path = vault_user_path.Append(kCacheDir);
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
  LoadSystemSalt(kImageDir);
  Mount mount;
  NiceMock<MockTpm> tpm;
  mount.get_crypto()->set_tpm(&tpm);
  mount.set_shadow_root(kImageDir);
  mount.set_use_tpm(false);

  NiceMock<MockPlatform> platform;
  mount.set_platform(&platform);

  EXPECT_TRUE(mount.Init());

  cryptohome::SecureBlob passkey;
  cryptohome::Crypto::PasswordToPasskey(kDefaultUsers[8].password,
                                        system_salt_, &passkey);
  UsernamePasskey up(kDefaultUsers[8].username, passkey);

  // As we don't have real mount in the test, immagine its output (home)
  // directory.
  FilePath home_dir(kHomeDir);
  file_util::CreateDirectory(home_dir);
  mount.set_home_dir(home_dir.value());

  // Pretend that mounted cryptohome already had non-pass-through
  // subdirs "Cache" and "Downloads".
  FilePath cache_dir(home_dir.Append(kCacheDir));
  FilePath downloads_dir(home_dir.Append(kDownloadsDir));
  LOG(INFO) << "mkcache: " << cache_dir.value() << " "
            << file_util::CreateDirectory(cache_dir);
  LOG(INFO) << "mkdownload: " << downloads_dir.value() << " "
            << file_util::CreateDirectory(downloads_dir);

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
  EXPECT_CALL(platform, Bind(_, _))
      .WillRepeatedly(Return(true));
  Mount::MountError error;
  EXPECT_TRUE(mount.MountCryptohome(up, Mount::MountArgs(), &error));

  // Check that vault path now have pass-through version of tracked dirs.
  FilePath image_dir(kImageDir);
  FilePath user_path = image_dir.Append(up.GetObfuscatedUsername(system_salt_));
  FilePath vault_path = user_path.Append(kVaultDir);
  FilePath vault_user_path = vault_path.Append(kUserDir);
  ASSERT_TRUE(file_util::PathExists(vault_user_path.Append(kCacheDir)));
  ASSERT_TRUE(file_util::PathExists(vault_user_path.Append(kDownloadsDir)));

  // Check that vault path does not contain user data unencrypted.
  // Note, that if we had real mount, we would see encrypted file names there;
  // but with our mock mount, we must see empty directories.
  EXPECT_TRUE(file_util::IsDirectoryEmpty(vault_path.Append(kCacheDir)));
  EXPECT_TRUE(file_util::IsDirectoryEmpty(vault_path.Append(kDownloadsDir)));

  // Check that Downloads is completely migrated.
  // TODO(ellyjones): figure out how to check that the migrated-from directories
  // are now empty
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
}

TEST_F(MountTest, UserActivityTimestampUpdated) {
  // checks that user activity timestamp is updated during Mount() and
  // periodically while mounted, other Keyset fields remains the same
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
  cryptohome::Crypto::PasswordToPasskey(kDefaultUsers[9].password,
                                        system_salt_, &passkey);
  UsernamePasskey up(kDefaultUsers[9].username, passkey);

  // Mount()
  Mount::MountError error;
  EXPECT_CALL(platform, Mount(_, _, _, _))
      .WillOnce(Return(true));
  EXPECT_CALL(platform, Bind(_, _))
      .Times(3)
      .WillRepeatedly(Return(true));
  ASSERT_TRUE(mount.MountCryptohome(up, Mount::MountArgs(), &error));

  // Update the timestamp. Normally it is called in MountTaskMount::Run() in
  // background but here in the test we must call it manually.
  static const int kMagicTimestamp = 123;
  EXPECT_CALL(platform, GetCurrentTime())
      .WillOnce(Return(base::Time::FromInternalValue(kMagicTimestamp)));
  mount.UpdateCurrentUserActivityTimestamp(0);
  SerializedVaultKeyset serialized1;
  ASSERT_TRUE(mount.LoadVaultKeyset(up, &serialized1));

  // Check that last activity timestamp is updated.
  ASSERT_TRUE(serialized1.has_last_activity_timestamp());
  EXPECT_EQ(kMagicTimestamp, serialized1.last_activity_timestamp());

  // Unmount the user. This must update user's activity timestamps.
  static const int kMagicTimestamp2 = 234;
  EXPECT_CALL(platform, GetCurrentTime())
      .WillOnce(Return(base::Time::FromInternalValue(kMagicTimestamp2)));
  EXPECT_CALL(platform, Unmount(_, _, _))
      .Times(4)
      .WillRepeatedly(Return(true));
  mount.UnmountCryptohome();
  SerializedVaultKeyset serialized2;
  ASSERT_TRUE(mount.LoadVaultKeyset(up, &serialized2));
  ASSERT_TRUE(serialized2.has_last_activity_timestamp());
  EXPECT_EQ(kMagicTimestamp2, serialized2.last_activity_timestamp());

  // Update timestamp again, after user is unmounted. User's activity
  // timestamp must not change this.
  mount.UpdateCurrentUserActivityTimestamp(0);
  SerializedVaultKeyset serialized3;
  ASSERT_TRUE(mount.LoadVaultKeyset(up, &serialized3));
  ASSERT_TRUE(serialized3.has_last_activity_timestamp());
  EXPECT_EQ(serialized3.has_last_activity_timestamp(),
            serialized2.has_last_activity_timestamp());
}

// Users for testing automatic disk cleanup.
const TestUserInfo kAlternateUsers[] = {
  {"user0@invalid.domain", "zero", true, false},
  {"user1@invalid.domain", "odin", true, false},
  {"user2@invalid.domain", "dwaa", true, false},
  {"owner@invalid.domain", "1234", true, false},
};
const int kAlternateUserCount = arraysize(kAlternateUsers);
const char kAltImageDir[] = "alt_test_image_dir";

// Checks DoAutomaticFreeDiskSpaceControl() to act in different situations when
// free disk space is low.
class DoAutomaticFreeDiskSpaceControlTest : public MountTest {
 public:
  DoAutomaticFreeDiskSpaceControlTest() { }

  void SetUp() {
    // Set up fresh users.
    cryptohome::MakeTests make_tests;
    make_tests.InitTestData(kAltImageDir, kAlternateUsers, kAlternateUserCount);
    MountTest::SetUp();
    LoadSystemSalt(kAltImageDir);
    FilePath root_dir(kAltImageDir);
    for (int user = 0; user != kAlternateUserCount; user++) {
      cryptohome::SecureBlob passkey;
      cryptohome::Crypto::PasswordToPasskey(kAlternateUsers[user].password,
                                            system_salt_, &passkey);
      username_passkey_[user].Assign(
          UsernamePasskey(kAlternateUsers[user].username, passkey));
      image_path_[user] = root_dir.Append(
          username_passkey_[user].GetObfuscatedUsername(system_salt_));
    }

    // Initialize Mount object.
    mount_.get_crypto()->set_tpm(&tpm_);
    mount_.set_shadow_root(kAltImageDir);
    mount_.set_use_tpm(false);
    mount_.set_platform(&platform_);
    EXPECT_TRUE(mount_.Init());
  }

  bool StoreSerializedKeyset(
      const string& key_path,
      const SerializedVaultKeyset& serialized) {
    SecureBlob final_blob(serialized.ByteSize());
    serialized.SerializeWithCachedSizesToArray(
        static_cast<google::protobuf::uint8*>(final_blob.data()));
    unsigned int data_written = file_util::WriteFile(
        FilePath(key_path),
        static_cast<const char*>(final_blob.const_data()),
        final_blob.size());
    return data_written == final_blob.size();
  }

  // Set the user with specified |key_file| old.
  bool SetUserTimestamp(const Mount& mount, int user, base::Time timestamp) {
    CHECK(user >= 0 && user < kAlternateUserCount);
    const string key_file =
        mount.GetUserKeyFileForUser(image_path_[user].BaseName().value());
    SerializedVaultKeyset serialized;
    if (!LoadSerializedKeyset(key_file, &serialized))
      return false;
    serialized.set_last_activity_timestamp(timestamp.ToInternalValue());
    return StoreSerializedKeyset(key_file, serialized);
  }

 protected:
  Mount mount_;
  NiceMock<MockTpm> tpm_;
  NiceMock<MockPlatform> platform_;
  FilePath image_path_[kAlternateUserCount];
  UsernamePasskey username_passkey_[kAlternateUserCount];

 private:
  DISALLOW_COPY_AND_ASSIGN(DoAutomaticFreeDiskSpaceControlTest);
};

TEST_F(DoAutomaticFreeDiskSpaceControlTest, CacheCleanup) {
  // Removes caches of all users (except current one, if any).

  // For every user, prepare cryptohome contents.
  const string contents = "some encrypted contents";
  FilePath cache_dir[kAlternateUserCount];
  FilePath cache_subdir[kAlternateUserCount];
  for (int user = 0; user != kAlternateUserCount; user++) {
    // Let their Cache dirs be filled with some data.
    cache_dir[user] = image_path_[user].Append(kVaultDir).Append(kCacheDir);
    file_util::CreateDirectory(cache_dir[user]);
    file_util::WriteFile(cache_dir[user].Append("cached_file"),
                         contents.c_str(), contents.length());
    cache_subdir[user] = cache_dir[user].Append("cache_subdir");
    file_util::CreateDirectory(cache_subdir[user]);
    file_util::WriteFile(cache_subdir[user].Append("cached_file"),
                         contents.c_str(), contents.length());
  }

  // Firstly, pretend we have lots of free space.
  EXPECT_CALL(platform_, AmountOfFreeDiskSpace(_))
      .WillRepeatedly(Return(kMinFreeSpace + 1));
  EXPECT_FALSE(mount_.DoAutomaticFreeDiskSpaceControl());

  // Check that Cache is not changed.
  for (int user = 0; user != kAlternateUserCount; user++) {
    string tested;
    EXPECT_TRUE(file_util::PathExists(cache_dir[user]));
    EXPECT_TRUE(file_util::ReadFileToString(
        cache_dir[user].Append("cached_file"), &tested));
    EXPECT_EQ(contents, tested);
    EXPECT_TRUE(file_util::PathExists(cache_subdir[user]));
    tested.clear();
    EXPECT_TRUE(file_util::ReadFileToString(
        cache_subdir[user].Append("cached_file"), &tested));
    EXPECT_EQ(contents, tested);
  }

  // Now pretend we have lack of free space.
  EXPECT_CALL(platform_, AmountOfFreeDiskSpace(_))
      .WillOnce(Return(kMinFreeSpace - 1))
      .WillRepeatedly(Return(kEnoughFreeSpace));
  EXPECT_TRUE(mount_.DoAutomaticFreeDiskSpaceControl());

  // Cache must be empty (and not even be deleted).
  for (int user = 0; user != kAlternateUserCount; user++) {
    EXPECT_TRUE(file_util::IsDirectoryEmpty(cache_dir[user]));
    EXPECT_TRUE(file_util::PathExists(cache_dir[user]));

    // Check that we did not leave any litter.
    file_util::Delete(cache_dir[user], true);
    EXPECT_TRUE(file_util::IsDirectoryEmpty(
        image_path_[user].Append(kVaultDir)));
  }
}

TEST_F(DoAutomaticFreeDiskSpaceControlTest, OldUsersCleanupNoTimestamp) {
  // Removes old (except owner and the current one, if any) even if
  // users had no oldest activity timestamp.

  // Setting owner so that old user may be deleted.
  mount_.SetOwnerUser("owner@invalid.domain");

  // Verify that user timestamp cache must be not initialized by now.
  UserOldestActivityTimestampCache* user_timestamp =
      mount_.user_timestamp_cache();
  EXPECT_FALSE(user_timestamp->initialized());

  // Now pretend we have lack of free space.
  EXPECT_CALL(platform_, AmountOfFreeDiskSpace(_))
      .WillOnce(Return(kMinFreeSpace - 1))
      .WillRepeatedly(Return(kEnoughFreeSpace - 1));
  EXPECT_TRUE(mount_.DoAutomaticFreeDiskSpaceControl());

  // Make sure no users actually deleted as we didn't put
  // user timestamps, all users must remain.
  for (int user = 0; user != kAlternateUserCount; user++)
    EXPECT_TRUE(file_util::PathExists(image_path_[user]));

  // Verify that user timestamp cache must be initialized by now.
  EXPECT_TRUE(user_timestamp->initialized());

  // Simulate the user[0] have been updated but not old enough.
  user_timestamp->UpdateExistingUser(
      image_path_[0], base::Time::Now() - kOldUserLastActivityTime / 2);

  // Now pretend we have lack of free space.
  EXPECT_CALL(platform_, AmountOfFreeDiskSpace(_))
      .WillOnce(Return(kMinFreeSpace - 1))
      .WillRepeatedly(Return(kEnoughFreeSpace - 1));
  EXPECT_TRUE(mount_.DoAutomaticFreeDiskSpaceControl());

  // Make sure no users actually deleted. Because the only
  // timestamp we put is not old enough.
  for (int user = 0; user != kAlternateUserCount; user++)
    EXPECT_TRUE(file_util::PathExists(image_path_[user]));

  // Verify that user timestamp cache must be initialized.
  EXPECT_TRUE(user_timestamp->initialized());

  // Simulate the user[0] have been updated old enough.
  user_timestamp->UpdateExistingUser(
      image_path_[0], base::Time::Now() - kOldUserLastActivityTime);

  // Now pretend we have lack of free space.
  EXPECT_CALL(platform_, AmountOfFreeDiskSpace(_))
      .WillOnce(Return(kMinFreeSpace - 1))
      .WillRepeatedly(Return(kEnoughFreeSpace - 1));
  EXPECT_TRUE(mount_.DoAutomaticFreeDiskSpaceControl());

  // User[0] is old, user[1,2] have no timestamp => older, user[3] is owner.
  EXPECT_FALSE(file_util::PathExists(image_path_[0]));
  EXPECT_FALSE(file_util::PathExists(image_path_[1]));
  EXPECT_FALSE(file_util::PathExists(image_path_[2]));
  EXPECT_TRUE(file_util::PathExists(image_path_[3]));
}

TEST_F(DoAutomaticFreeDiskSpaceControlTest, OldUsersCleanup) {
  // Remove old users, oldest first. Stops removing when disk space is enough.

  // Setting owner so that old user may be deleted.
  mount_.SetOwnerUser("owner@invalid.domain");

  // Update cached users with following timestamps:
  // user[0] is old, user[1] is up to date, user[2] still have no timestamp,
  // user[3] is very old, but it is an owner.
  SetUserTimestamp(mount_, 0, base::Time::Now() - kOldUserLastActivityTime);
  SetUserTimestamp(mount_, 1, base::Time::Now());
  SetUserTimestamp(mount_, 3, base::Time::Now() - kOldUserLastActivityTime * 2);

  // Now pretend we have lack of free space 2 times.
  // So at 1st Caches are deleted and then 1 oldest user is deleted.
  EXPECT_CALL(platform_, AmountOfFreeDiskSpace(_))
      .WillOnce(Return(kMinFreeSpace - 1))
      .WillOnce(Return(kEnoughFreeSpace - 1))
      .WillRepeatedly(Return(kEnoughFreeSpace));
  EXPECT_TRUE(mount_.DoAutomaticFreeDiskSpaceControl());

  // User[2] should be deleted because we have not updated its
  // timestamp (so it does not have one) and 1st user is old, so 2nd
  // user is older.
  EXPECT_TRUE(file_util::PathExists(image_path_[0]));
  EXPECT_TRUE(file_util::PathExists(image_path_[1]));
  EXPECT_FALSE(file_util::PathExists(image_path_[2]));
  EXPECT_TRUE(file_util::PathExists(image_path_[3]));

  // Now pretend we have lack of free space at all times.
  EXPECT_CALL(platform_, AmountOfFreeDiskSpace(_))
      .WillOnce(Return(kMinFreeSpace - 1))
      .WillRepeatedly(Return(kEnoughFreeSpace - 1));
  EXPECT_TRUE(mount_.DoAutomaticFreeDiskSpaceControl());

  // User[0] should be deleted because it is oldest now.
  // User[1] should not be deleted because it is up to date.
  EXPECT_FALSE(file_util::PathExists(image_path_[0]));
  EXPECT_TRUE(file_util::PathExists(image_path_[1]));
  EXPECT_FALSE(file_util::PathExists(image_path_[2]));
  EXPECT_TRUE(file_util::PathExists(image_path_[3]));
}

TEST_F(DoAutomaticFreeDiskSpaceControlTest, OldUsersCleanupWithRestart) {
  // Cryptohomed may restart for some reason and continue nuking users
  // as if not restarted. Scenario is same as in test OldUsersCleanup.

  // Update cached users with following timestamps:
  // user[0] is old, user[1] is up to date, user[2] still have no timestamp,
  // user[3] is very old, but it is an owner.
  SetUserTimestamp(mount_, 0, base::Time::Now() - kOldUserLastActivityTime);
  SetUserTimestamp(mount_, 1, base::Time::Now());
  SetUserTimestamp(mount_, 3, base::Time::Now() - kOldUserLastActivityTime * 2);

  // Setting owner so that old user may be deleted.
  mount_.SetOwnerUser("owner@invalid.domain");

  // Now pretend we have lack of free space 2 times.
  // So at 1st Caches are deleted and then 1 oldest user is deleted.
  EXPECT_CALL(platform_, AmountOfFreeDiskSpace(_))
      .WillOnce(Return(kMinFreeSpace - 1))
      .WillOnce(Return(kEnoughFreeSpace - 1))
      .WillRepeatedly(Return(kEnoughFreeSpace));
  EXPECT_TRUE(mount_.DoAutomaticFreeDiskSpaceControl());

  // User[2] should be deleted because we have not updated its
  // timestamp (so it does not have one) and 1st user is old, so 2nd
  // user is older.
  EXPECT_TRUE(file_util::PathExists(image_path_[0]));
  EXPECT_TRUE(file_util::PathExists(image_path_[1]));
  EXPECT_FALSE(file_util::PathExists(image_path_[2]));
  EXPECT_TRUE(file_util::PathExists(image_path_[3]));

  // Forget about mount_ instance as if it has crashed.
  // Simulate cryptohome restart. Create new Mount instance.
  scoped_ptr<Mount> mount2(new Mount);
  mount2->get_crypto()->set_tpm(&tpm_);
  mount2->set_shadow_root(kAltImageDir);
  mount2->set_use_tpm(false);
  mount2->set_platform(&platform_);
  EXPECT_TRUE(mount2->Init());

  // Setting owner so that old user may be deleted. Currently chrome
  // will set it on nearest log in.
  mount2->SetOwnerUser("owner@invalid.domain");

  // Now pretend we have lack of free space at all times.
  EXPECT_CALL(platform_, AmountOfFreeDiskSpace(_))
      .WillOnce(Return(kMinFreeSpace - 1))
      .WillRepeatedly(Return(kEnoughFreeSpace - 1));
  EXPECT_TRUE(mount2->DoAutomaticFreeDiskSpaceControl());

  // User[0] should be deleted because it is oldest now.
  // User[1] should not be deleted because it is up to date.
  EXPECT_FALSE(file_util::PathExists(image_path_[0]));
  EXPECT_TRUE(file_util::PathExists(image_path_[1]));
  EXPECT_FALSE(file_util::PathExists(image_path_[2]));
  EXPECT_TRUE(file_util::PathExists(image_path_[3]));
}

TEST_F(DoAutomaticFreeDiskSpaceControlTest, OldUsersCleanupNoOwnerSet) {
  // No users deleted when no owner known (set) and not in enterprise mode.

  // Update cached users with artificial timestamp: user[0] is old,
  // Other users still have no timestamp so we consider them even older.
  ASSERT_TRUE(SetUserTimestamp(
      mount_, 0,  base::Time::Now() - kOldUserLastActivityTime));

  // Now pretend we have lack of free space at all times - to delete all users.
  EXPECT_CALL(platform_, AmountOfFreeDiskSpace(_))
      .WillOnce(Return(kMinFreeSpace - 1))
      .WillRepeatedly(Return(kEnoughFreeSpace - 1));
  EXPECT_TRUE(mount_.DoAutomaticFreeDiskSpaceControl());

  // All users must remain because, although they are either old or with no
  // timestamp, we have not set an owner or enterprise mode.
  EXPECT_TRUE(file_util::PathExists(image_path_[0]));
  EXPECT_TRUE(file_util::PathExists(image_path_[1]));
  EXPECT_TRUE(file_util::PathExists(image_path_[2]));
  EXPECT_TRUE(file_util::PathExists(image_path_[3]));
}

TEST_F(DoAutomaticFreeDiskSpaceControlTest, OldUsersCleanupEnterprise) {
  // Removes old users in enterprise mode.

  // Setting owner so that old user may be deleted.
  mount_.SetOwnerUser("owner@invalid.domain");
  mount_.set_enterprise_owned(true);

  // Update cached users with artificial timestamp: user[0] is old,
  // Other users still have no timestamp so we consider them even older.
  ASSERT_TRUE(SetUserTimestamp(
      mount_, 0,  base::Time::Now() - kOldUserLastActivityTime));

  // Now pretend we have lack of free space at all times - to delete all users.
  EXPECT_CALL(platform_, AmountOfFreeDiskSpace(_))
      .WillOnce(Return(kMinFreeSpace - 1))
      .WillRepeatedly(Return(kEnoughFreeSpace - 1));
  EXPECT_TRUE(mount_.DoAutomaticFreeDiskSpaceControl());

  // All users must be deleted because they are either old or with no
  // timestamp. Owner is not counted because we are in enterprise
  // mode.
  EXPECT_FALSE(file_util::PathExists(image_path_[0]));
  EXPECT_FALSE(file_util::PathExists(image_path_[1]));
  EXPECT_FALSE(file_util::PathExists(image_path_[2]));
  EXPECT_FALSE(file_util::PathExists(image_path_[3]));
}

TEST_F(DoAutomaticFreeDiskSpaceControlTest, OldUsersCleanupWhenMounted) {
  // Do not remove currently mounted user and do remove it when unmounted.

  // Setting owner (user[3]) so that old user may be deleted.
  mount_.SetOwnerUser("owner@invalid.domain");

  // Set all users old should one of them have a timestamp.
  mount_.set_old_user_last_activity_time(base::TimeDelta::FromMicroseconds(0));
  SetUserTimestamp(mount_, 3, base::Time::Now() - kOldUserLastActivityTime);

  // Mount() user[0].
  Mount::MountError error;
  EXPECT_CALL(platform_, Mount(_, _, _, _))
      .WillOnce(Return(true));
  EXPECT_CALL(platform_, Bind(_, _))
      .Times(3)
      .WillRepeatedly(Return(true));
  ASSERT_TRUE(mount_.MountCryptohome(
      username_passkey_[0], Mount::MountArgs(), &error));
  const string current_uservault = image_path_[0].Append(kVaultDir).value();
  LOG(WARNING) << "User[0]: " << current_uservault;

  // Update current user timestamp.
  // Normally it is done in MountTaskMount::Run() in background.
  mount_.UpdateCurrentUserActivityTimestamp(0);

  // Now pretend we have lack of free space.
  EXPECT_CALL(platform_, AmountOfFreeDiskSpace(_))
      .WillOnce(Return(kMinFreeSpace - 1))
      .WillRepeatedly(Return(kEnoughFreeSpace - 1));
  EXPECT_CALL(platform_, IsDirectoryMountedWith(_, _))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(platform_, IsDirectoryMountedWith(_, current_uservault))
      .WillRepeatedly(Return(true));
  EXPECT_TRUE(mount_.DoAutomaticFreeDiskSpaceControl());

  // User[0] should not be deleted because it is the current,
  // user[1,2] should be deleted because they are old.
  // user[3] should not be deleted because it is the oener.
  EXPECT_TRUE(file_util::PathExists(image_path_[0]));
  EXPECT_FALSE(file_util::PathExists(image_path_[1]));
  EXPECT_FALSE(file_util::PathExists(image_path_[2]));
  EXPECT_TRUE(file_util::PathExists(image_path_[3]));

  // Now unmount the user. So it (user[0]) should be cached and may be
  // deleted next when it becomes old.
  EXPECT_CALL(platform_, Unmount(_, _, _))
      .Times(4)
      .WillRepeatedly(Return(true));
  mount_.UnmountCryptohome();

  // Now pretend we have lack of free space.
  EXPECT_CALL(platform_, AmountOfFreeDiskSpace(_))
      .WillOnce(Return(kMinFreeSpace - 1))
      .WillRepeatedly(Return(kEnoughFreeSpace - 1));
  EXPECT_CALL(platform_, IsDirectoryMountedWith(_, _))
      .WillRepeatedly(Return(false));
  EXPECT_TRUE(mount_.DoAutomaticFreeDiskSpaceControl());

  // User[0] should be deleted because it is no more current and we
  // delete all users despite their oldness in this test.
  EXPECT_FALSE(file_util::PathExists(image_path_[0]));
  EXPECT_FALSE(file_util::PathExists(image_path_[1]));
  EXPECT_FALSE(file_util::PathExists(image_path_[2]));
  EXPECT_TRUE(file_util::PathExists(image_path_[3]));
}

TEST_F(MountTest, MountForUserOrderingTest) {
  // Checks that mounts made with MountForUser/BindForUser are undone in the
  // right order.
  InSequence sequence;
  Mount mount;
  NiceMock<MockTpm> tpm;
  NiceMock<MockPlatform> platform;
  mount.set_platform(&platform);

  mount.get_crypto()->set_tpm(&tpm);
  mount.set_shadow_root(kImageDir);
  mount.set_skel_source(kSkelDir);
  mount.set_use_tpm(false);
  EXPECT_TRUE(mount.Init());
  UserSession session;
  Crypto crypto;
  SecureBlob salt;
  salt.resize(16);
  crypto.GetSecureRandom(static_cast<unsigned char*>(salt.data()), salt.size());
  session.Init(&crypto, salt);
  UsernamePasskey up("username", SecureBlob("password", 8));
  EXPECT_TRUE(session.SetUser(up));


  std::string src = "/src";
  std::string dest0 = "/dest/foo";
  std::string dest1 = "/dest/bar";
  std::string dest2 = "/dest/baz";
  EXPECT_CALL(platform, Mount(src, dest0, _, _))
      .WillOnce(Return(true));
  EXPECT_CALL(platform, Bind(src, dest1))
      .WillOnce(Return(true));
  EXPECT_CALL(platform, Mount(src, dest2, _, _))
      .WillOnce(Return(true));
  EXPECT_CALL(platform, Unmount(dest2, _, _))
      .WillOnce(Return(true));
  EXPECT_CALL(platform, Unmount(dest1, _, _))
      .WillOnce(Return(true));
  EXPECT_CALL(platform, Unmount(dest0, _, _))
      .WillOnce(Return(true));

  EXPECT_TRUE(mount.MountForUser(&session, src, dest0, "", ""));
  EXPECT_TRUE(mount.BindForUser(&session, src, dest1));
  EXPECT_TRUE(mount.MountForUser(&session, src, dest2, "", ""));
  mount.UnmountAllForUser(&session);
  EXPECT_FALSE(mount.UnmountForUser(&session));
}

}  // namespace cryptohome
