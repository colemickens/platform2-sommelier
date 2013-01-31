// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit tests for Mount.

#include "mount.h"

#include <openssl/evp.h>
#include <openssl/sha.h>
#include <pwd.h>
#include <string.h>  // For memset(), memcpy()
#include <stdlib.h>
#include <sys/types.h>
#include <vector>

#include <base/file_path.h>
#include <base/file_util.h>
#include <base/logging.h>
#include <base/time.h>
#include <base/stringprintf.h>
#include <chromeos/cryptohome.h>
#include <chromeos/secure_blob.h>
#include <chromeos/utility.h>
#include <gtest/gtest.h>
#include <policy/libpolicy.h>
#include <policy/mock_device_policy.h>

#include "crypto.h"
#include "cryptolib.h"
#include "homedirs.h"
#include "make_tests.h"
#include "mock_platform.h"
#include "mock_tpm.h"
#include "mock_user_session.h"
#include "username_passkey.h"
#include "vault_keyset.h"
#include "vault_keyset.pb.h"

namespace cryptohome {
using chromeos::SecureBlob;
using std::string;
using ::testing::AllOf;
using ::testing::AnyNumber;
using ::testing::AnyOf;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Not;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SetArgumentPointee;
using ::testing::StartsWith;
using ::testing::Unused;
using ::testing::_;

const char kImageDir[] = "test_image_dir";
const char kImageSaltFile[] = "test_image_dir/salt";
const char kSkelDir[] = "test_image_dir/skel";
const char kUserDir[] = "user";
const char kSalt[] = "1234567890";
const gid_t kDaemonGid = 400; // TODO: expose this in mount.h

ACTION_P2(SetOwner, owner_known, owner) {
  if (owner_known)
    *arg0 = owner;
  return owner_known;
}

ACTION_P(SetEphemeralUsersEnabled, ephemeral_users_enabled) {
  *arg0 = ephemeral_users_enabled;
  return true;
}

// Straight pass through.
bool TpmPassthroughEncrypt(const chromeos::SecureBlob &plaintext, Unused,
                           chromeos::SecureBlob *ciphertext, Unused) {
  ciphertext->resize(plaintext.size());
  memcpy(ciphertext->data(), plaintext.const_data(), plaintext.size());
  return true;
}

bool TpmPassthroughDecrypt(const chromeos::SecureBlob &ciphertext, Unused,
                           chromeos::SecureBlob *plaintext, Unused) {
  plaintext->resize(ciphertext.size());
  memcpy(plaintext->data(), ciphertext.const_data(), ciphertext.size());
  return true;
}

class MountTest : public ::testing::Test {
 public:
  MountTest() { }
  virtual ~MountTest() { }

  void SetUp() {
    // Populate the system salt
    helper_.SetUpSystemSalt();

    // Setup default uid/gid values
    chronos_uid_ = 1000;
    chronos_gid_ = 1000;
    shared_gid_ = 1001;
    chaps_uid_ = 223;
  }

  void TearDown() {
    helper_.TearDownSystemSalt();
  }

  void InsertTestUsers(const TestUserInfo* user_info_list, int count) {
    helper_.InitTestData(kImageDir, user_info_list,
                         static_cast<size_t>(count));
  }

  bool DoMountInit(Mount* mount) {
    MockPlatform* platform = static_cast<MockPlatform *>(mount->platform());
    EXPECT_CALL(*platform, GetUserId("chronos", _, _))
      .WillOnce(DoAll(SetArgumentPointee<1>(chronos_uid_),
                      SetArgumentPointee<2>(chronos_gid_),
                      Return(true)));
    EXPECT_CALL(*platform, GetUserId("chaps", _, _))
      .WillOnce(DoAll(SetArgumentPointee<1>(chaps_uid_),
                      SetArgumentPointee<2>(shared_gid_),
                      Return(true)));
    EXPECT_CALL(*platform, GetGroupId("chronos-access", _))
      .WillOnce(DoAll(SetArgumentPointee<1>(shared_gid_),
                      Return(true)));
    helper_.InjectSystemSalt(platform, kImageSaltFile);
    return mount->Init();
  }

  bool LoadSerializedKeyset(const chromeos::Blob& contents,
                            cryptohome::SerializedVaultKeyset* serialized) {
    CHECK(contents.size() != 0);
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

  void set_policy(Mount* mount,
                  bool owner_known,
                  const string& owner,
                  bool ephemeral_users_enabled) {
    policy::MockDevicePolicy* device_policy = new policy::MockDevicePolicy();
    EXPECT_CALL(*device_policy, LoadPolicy())
        .Times(AnyNumber())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*device_policy, GetOwner(_))
        .WillRepeatedly(SetOwner(owner_known, owner));
    EXPECT_CALL(*device_policy, GetEphemeralUsersEnabled(_))
        .WillRepeatedly(SetEphemeralUsersEnabled(ephemeral_users_enabled));
    mount->set_policy_provider(new policy::PolicyProvider(device_policy));
  }

 protected:
  // Protected for trivial access
  MakeTests helper_;
  uid_t chronos_uid_;
  gid_t chronos_gid_;
  uid_t chaps_uid_;
  gid_t shared_gid_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MountTest);
};

TEST_F(MountTest, BadInitTest) {
  // create a Mount instance that points to a bad shadow root
  Mount mount;
  NiceMock<MockTpm> tpm;
  mount.crypto()->set_tpm(&tpm);
  mount.set_shadow_root("/dev/null");
  mount.set_skel_source(kSkelDir);
  mount.set_use_tpm(false);
  MockPlatform platform;
  mount.set_platform(&platform);
  mount.crypto()->set_platform(&platform);
  set_policy(&mount, false, "", false);

  SecureBlob passkey;
  cryptohome::Crypto::PasswordToPasskey(kDefaultUsers[0].password,
                                        helper_.system_salt, &passkey);
  UsernamePasskey up(kDefaultUsers[0].username, passkey);

  // shadow root creation should fail
  EXPECT_CALL(platform, DirectoryExists("/dev/null"))
    .Times(2)
    .WillRepeatedly(Return(false));
  EXPECT_CALL(platform, CreateDirectory("/dev/null"))
    .Times(2)
    .WillRepeatedly(Return(false));
  // salt creation failure because shadow_root is bogus
  EXPECT_CALL(platform, FileExists("/dev/null/salt"))
    .Times(2)
    .WillRepeatedly(Return(false));
  EXPECT_CALL(platform, WriteFile("/dev/null/salt", _))
    .Times(2)
    .WillRepeatedly(Return(false));
  EXPECT_CALL(platform, GetUserId("chronos", _, _))
    .WillOnce(DoAll(SetArgumentPointee<1>(1000), SetArgumentPointee<2>(1000),
                    Return(true)));
  EXPECT_CALL(platform, GetUserId("chaps", _, _))
    .WillOnce(DoAll(SetArgumentPointee<1>(1001), SetArgumentPointee<2>(1001),
                    Return(true)));
  EXPECT_CALL(platform, GetGroupId("chronos-access", _))
    .WillOnce(DoAll(SetArgumentPointee<1>(1002), Return(true)));
  EXPECT_FALSE(mount.Init());
  ASSERT_FALSE(mount.AreValid(up));
}

TEST_F(MountTest, CurrentCredentialsTest) {
  // create a Mount instance that points to a good shadow root, test that it
  // properly authenticates against the first key
  Mount mount;
  NiceMock<MockTpm> tpm;
  NiceMock<MockPlatform> platform;
  mount.crypto()->set_tpm(&tpm);
  mount.crypto()->set_platform(&platform);
  mount.set_shadow_root(kImageDir);
  mount.set_skel_source(kSkelDir);
  mount.set_use_tpm(false);
  set_policy(&mount, false, "", false);

  SecureBlob passkey;
  cryptohome::Crypto::PasswordToPasskey(kDefaultUsers[3].password,
                                        helper_.system_salt, &passkey);
  UsernamePasskey up(kDefaultUsers[3].username, passkey);

  helper_.InjectSystemSalt(&platform, kImageSaltFile);
  EXPECT_TRUE(mount.Init());

  NiceMock<MockUserSession> user_session;
  user_session.Init(SecureBlob());
  user_session.SetUser(up);
  mount.set_current_user(&user_session);

  EXPECT_CALL(user_session, CheckUser(_))
      .WillOnce(Return(true));
  EXPECT_CALL(user_session, Verify(_))
      .WillOnce(Return(true));

  ASSERT_TRUE(mount.AreValid(up));
}

TEST_F(MountTest, BadDecryptTest) {
  // create a Mount instance that points to a good shadow root, test that it
  // properly denies access with a bad passkey
  Mount mount;
  NiceMock<MockTpm> tpm;
  mount.crypto()->set_tpm(&tpm);
  mount.set_shadow_root(kImageDir);
  mount.set_skel_source(kSkelDir);
  mount.set_use_tpm(false);
  set_policy(&mount, false, "", false);

  SecureBlob passkey;
  cryptohome::Crypto::PasswordToPasskey("bogus", helper_.system_salt, &passkey);
  UsernamePasskey up(kDefaultUsers[4].username, passkey);

  EXPECT_TRUE(mount.Init());
  ASSERT_FALSE(mount.AreValid(up));
}

TEST_F(MountTest, CheckChapsDirectoryCalledWithExistingDir) {
  Mount mount;
  NiceMock<MockPlatform> platform;
  NiceMock<MockTpm> tpm;
  mount.crypto()->set_tpm(&tpm);
  mount.crypto()->set_platform(&platform);
  mount.set_shadow_root(kImageDir);
  mount.set_skel_source(kSkelDir);
  mount.set_use_tpm(false);
  mount.set_platform(&platform);
  chaps_uid_ = 101010;
  shared_gid_ = 101010;
  EXPECT_TRUE(DoMountInit(&mount));
  EXPECT_CALL(platform, DirectoryExists(_))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(platform, GetOwnership(_, _, _))
      .WillRepeatedly(DoAll(SetArgumentPointee<1>(chaps_uid_),
                            SetArgumentPointee<2>(shared_gid_),
                            Return(true)));
  bool permissions_status = false;
  EXPECT_TRUE(mount.CheckChapsDirectory(&permissions_status));
  EXPECT_TRUE(permissions_status);
}

TEST_F(MountTest, CheckChapsDirectoryCalledWithExistingDirWithBadPerms) {
  Mount mount;
  NiceMock<MockPlatform> platform;
  mode_t bad_perms = S_IXGRP;
  NiceMock<MockTpm> tpm;
  mount.crypto()->set_tpm(&tpm);
  mount.crypto()->set_platform(&platform);
  mount.set_shadow_root(kImageDir);
  mount.set_skel_source(kSkelDir);
  mount.set_use_tpm(false);
  mount.set_platform(&platform);
  EXPECT_TRUE(DoMountInit(&mount));
  EXPECT_CALL(platform, DirectoryExists(_))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(platform, GetPermissions(_, _))
      .WillRepeatedly(DoAll(SetArgumentPointee<1>(bad_perms), Return(true)));
  bool permissions_status = false;
  EXPECT_TRUE(mount.CheckChapsDirectory(&permissions_status));
  EXPECT_FALSE(permissions_status);
}

TEST_F(MountTest, CheckChapsDirectoryCalledWithExistingDirWithBadUID) {
  Mount mount;
  NiceMock<MockPlatform> platform;
  NiceMock<MockTpm> tpm;
  mount.crypto()->set_tpm(&tpm);
  mount.crypto()->set_platform(&platform);
  mount.set_shadow_root(kImageDir);
  mount.set_skel_source(kSkelDir);
  mount.set_use_tpm(false);
  mount.set_platform(&platform);
  uid_t bad_uid = 0;

  // Populate the chaps uid and gid.
  EXPECT_TRUE(DoMountInit(&mount));

  EXPECT_CALL(platform, DirectoryExists(_))
      .WillRepeatedly(Return(true));
  mode_t expected_perms = S_IRWXU | S_IRGRP | S_IXGRP;
  EXPECT_CALL(platform, GetPermissions("/home/chronos/user/.chaps", _))
    .WillOnce(DoAll(SetArgumentPointee<1>(expected_perms), Return(true)));
  EXPECT_CALL(platform, GetOwnership(_, _, _))
      .WillRepeatedly(DoAll(SetArgumentPointee<1>(bad_uid),
                            SetArgumentPointee<2>(shared_gid_),
                            Return(true)));

  bool permissions_status = false;
  EXPECT_TRUE(mount.CheckChapsDirectory(&permissions_status));
  EXPECT_FALSE(permissions_status);
}

TEST_F(MountTest, CheckChapsDirectoryCalledWithExistingDirWithBadGID) {
  Mount mount;
  NiceMock<MockPlatform> platform;
  NiceMock<MockTpm> tpm;
  mount.crypto()->set_tpm(&tpm);
  mount.crypto()->set_platform(&platform);
  mount.set_shadow_root(kImageDir);
  mount.set_skel_source(kSkelDir);
  mount.set_use_tpm(false);
  mount.set_platform(&platform);

  chaps_uid_ = 101010;
  shared_gid_ = 101011;
  gid_t bad_gid = 0;
  EXPECT_TRUE(DoMountInit(&mount));
  EXPECT_CALL(platform, DirectoryExists(_))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(platform, GetOwnership(_, _, _))
      .WillRepeatedly(DoAll(SetArgumentPointee<1>(chaps_uid_),
                            SetArgumentPointee<2>(bad_gid),
                            Return(true)));
  bool permissions_status = false;
  EXPECT_TRUE(mount.CheckChapsDirectory(&permissions_status));
  EXPECT_FALSE(permissions_status);
}

TEST_F(MountTest, CheckChapsDirectoryCalledWithNonexistingDirWithFatalError) {
  Mount mount;
  NiceMock<MockPlatform> platform;
  NiceMock<MockTpm> tpm;
  mount.crypto()->set_tpm(&tpm);
  mount.crypto()->set_platform(&platform);
  mount.set_shadow_root(kImageDir);
  mount.set_skel_source(kSkelDir);
  mount.set_use_tpm(false);
  mount.set_platform(&platform);
  helper_.InjectSystemSalt(&platform, kImageSaltFile);
  EXPECT_TRUE(mount.Init());
  EXPECT_CALL(platform, DirectoryExists(_))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(platform, CreateDirectory(_))
      .WillOnce(Return(false))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(platform, SetOwnership(_, _, _))
      .WillOnce(Return(false))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(platform, SetPermissions(_, _))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(platform, GetPermissions(_, _))
      .WillRepeatedly(Return(false));
  bool permissions_status = false;
  EXPECT_FALSE(mount.CheckChapsDirectory(&permissions_status));
  EXPECT_FALSE(permissions_status);
  EXPECT_FALSE(mount.CheckChapsDirectory(&permissions_status));
  EXPECT_FALSE(permissions_status);
  EXPECT_FALSE(mount.CheckChapsDirectory(&permissions_status));
  EXPECT_FALSE(permissions_status);
}

TEST_F(MountTest, CheckChapsDirectoryCalledWithExistingDirWithFatalError) {
  Mount mount;
  NiceMock<MockPlatform> platform;
  NiceMock<MockTpm> tpm;
  mount.crypto()->set_tpm(&tpm);
  mount.crypto()->set_platform(&platform);
  mount.set_shadow_root(kImageDir);
  mount.set_skel_source(kSkelDir);
  mount.set_use_tpm(false);
  mount.set_platform(&platform);
  helper_.InjectSystemSalt(&platform, kImageSaltFile);
  EXPECT_TRUE(mount.Init());
  EXPECT_CALL(platform, DirectoryExists(_))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(platform, GetPermissions(_, _))
      .WillOnce(Return(false))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(platform, GetOwnership(_, _, _))
      .WillRepeatedly(Return(false));
  bool permissions_status = false;
  EXPECT_FALSE(mount.CheckChapsDirectory(&permissions_status));
  EXPECT_FALSE(permissions_status);
  EXPECT_FALSE(mount.CheckChapsDirectory(&permissions_status));
  EXPECT_FALSE(permissions_status);
}

TEST_F(MountTest, CreateCryptohomeTest) {
  InsertTestUsers(&kDefaultUsers[5], 1);
  // creates a cryptohome and tests credentials
  Mount mount;
  HomeDirs homedirs;
  NiceMock<MockTpm> tpm;
  NiceMock<MockPlatform> platform;
  set_policy(&mount, false, "", false);
  mount.crypto()->set_tpm(&tpm);
  mount.set_shadow_root(kImageDir);
  mount.set_skel_source(kSkelDir);
  mount.set_use_tpm(false);
  homedirs.set_shadow_root(kImageDir);
  homedirs.set_platform(&platform);
  homedirs.set_crypto(mount.crypto());
  mount.crypto()->set_platform(&platform);
  mount.set_platform(&platform);

  TestUser *user = &helper_.users[0];
  UsernamePasskey up(user->username, user->passkey);

  EXPECT_TRUE(DoMountInit(&mount));
  EXPECT_TRUE(homedirs.Init());
  bool created;

  // TODO(wad) Make this into a UserDoesntExist() helper.
  EXPECT_CALL(platform, FileExists(user->image_path))
    .WillOnce(Return(false));
  EXPECT_CALL(platform,
      CreateDirectory(
        AnyOf(user->mount_prefix,
              user->user_mount_prefix,
              user->user_mount_path,
              user->root_mount_prefix,
              user->root_mount_path)))
    .Times(6)
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform, DirectoryExists(user->vault_path))
    .WillRepeatedly(Return(false));
  EXPECT_CALL(platform,
      CreateDirectory(AnyOf(user->vault_path, user->base_path)))
    .Times(2)
    .WillRepeatedly(Return(true));

  chromeos::Blob creds;
  EXPECT_CALL(platform, WriteFile(user->keyset_path, _))
    .WillOnce(DoAll(SaveArg<1>(&creds), Return(true)));

  ASSERT_TRUE(mount.EnsureCryptohome(up, &created));
  ASSERT_TRUE(created);
  ASSERT_NE(creds.size(), 0);
  ASSERT_FALSE(mount.AreValid(up));
  EXPECT_CALL(platform, ReadFile(user->keyset_path, _))
    .WillOnce(DoAll(SetArgumentPointee<1>(creds), Return(true)));
  ASSERT_TRUE(homedirs.AreCredentialsValid(up));
}

TEST_F(MountTest, GoodReDecryptTest) {
  InsertTestUsers(&kDefaultUsers[6], 1);
  // create a Mount instance that points to a good shadow root, test that it
  // properly re-authenticates against the first key
  HomeDirs homedirs;
  Mount mount;
  MockTpm tpm;
  NiceMock<MockPlatform> platform;
  Crypto crypto(&platform);
  crypto.set_use_tpm(true);
  crypto.set_tpm(&tpm);
  mount.set_use_tpm(true);
  mount.set_shadow_root(kImageDir);
  mount.set_skel_source(kSkelDir);
  mount.set_platform(&platform);
  mount.crypto()->set_tpm(&tpm);
  mount.crypto()->set_use_tpm(true);
  mount.crypto()->set_platform(&platform);
  set_policy(&mount, false, "", false);
  homedirs.set_shadow_root(kImageDir);
  homedirs.set_platform(&platform);
  homedirs.crypto()->set_platform(&platform);
  homedirs.crypto()->set_tpm(&tpm);
  homedirs.crypto()->set_use_tpm(true);

  TestUser *user = &helper_.users[0];
  UsernamePasskey up(user->username, user->passkey);

  EXPECT_CALL(tpm, Init(_, _))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(tpm, IsEnabled())
    .WillRepeatedly(Return(true));
  EXPECT_CALL(tpm, IsOwned())
    .WillRepeatedly(Return(true));
  EXPECT_CALL(tpm, IsConnected())
    .WillRepeatedly(Return(true));
  helper_.InjectSystemSalt(&platform, kImageSaltFile);
  EXPECT_TRUE(mount.Init());
  EXPECT_TRUE(homedirs.Init());

  // Load the pre-generated keyset
  std::string key_path = mount.GetUserKeyFile(up);
  cryptohome::SerializedVaultKeyset serialized;
  EXPECT_TRUE(serialized.ParseFromArray(
                  static_cast<const unsigned char*>(&user->credentials[0]),
                  user->credentials.size()));
  // Ensure we're starting from scrypt so we can test migrate to a mock-TPM.
  EXPECT_EQ((serialized.flags() & SerializedVaultKeyset::SCRYPT_WRAPPED),
            SerializedVaultKeyset::SCRYPT_WRAPPED);

  // Call DecryptVaultKeyset first, allowing migration (the test data is not
  // scrypt nor TPM wrapped) to a TPM-wrapped keyset
  crypto.Init();
  VaultKeyset vault_keyset(&platform, &crypto);
  MountError error = MOUNT_ERROR_NONE;
  // Inject the pre-generated, scrypt-wrapped keyset.
  EXPECT_CALL(platform, FileExists(user->keyset_path))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform, ReadFile(user->keyset_path, _))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(user->credentials),
                          Return(true)));
  EXPECT_CALL(platform, FileExists(user->salt_path))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform, ReadFile(user->salt_path, _))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(user->user_salt),
                          Return(true)));

  // Allow the "backup" to be written
  EXPECT_CALL(platform,
      FileExists(StringPrintf("%s.bak", user->keyset_path.c_str())))
    .Times(2)  // Second time is for Mount::DeleteCacheFiles()
    .WillRepeatedly(Return(false));
  EXPECT_CALL(platform,
      FileExists(StringPrintf("%s.bak", user->salt_path.c_str())))
    .Times(2)  // Second time is for Mount::DeleteCacheFiles()
    .WillRepeatedly(Return(false));

  EXPECT_CALL(platform,
      Move(user->keyset_path,
           StringPrintf("%s.bak", user->keyset_path.c_str())))
    .WillOnce(Return(true));
  EXPECT_CALL(platform,
      Move(user->salt_path, StringPrintf("%s.bak", user->salt_path.c_str())))
    .WillOnce(Return(true));

  // Create the "TPM-wrapped" value by letting it save the plaintext.
  EXPECT_CALL(tpm, Encrypt(_, _, _, _))
    .WillRepeatedly(Invoke(TpmPassthroughEncrypt));
  chromeos::SecureBlob fake_pub_key("A", 1);
  EXPECT_CALL(tpm, GetPublicKeyHash(_))
    .WillRepeatedly(DoAll(SetArgumentPointee<0>(fake_pub_key),
                          Return(Tpm::RetryNone)));

  chromeos::Blob migrated_keyset;
  EXPECT_CALL(platform, WriteFile(user->keyset_path, _))
    .WillOnce(DoAll(SaveArg<1>(&migrated_keyset), Return(true)));
  EXPECT_TRUE(mount.DecryptVaultKeyset(up, true, &vault_keyset, &serialized,
                                       &error));
  ASSERT_EQ(error, MOUNT_ERROR_NONE);
  ASSERT_NE(migrated_keyset.size(), 0);

  cryptohome::SerializedVaultKeyset serialized_tpm;
  EXPECT_TRUE(serialized_tpm.ParseFromArray(
                  static_cast<const unsigned char*>(&migrated_keyset[0]),
                  migrated_keyset.size()));
  // Did it migrate?
  EXPECT_EQ((serialized_tpm.flags() & SerializedVaultKeyset::TPM_WRAPPED),
            SerializedVaultKeyset::TPM_WRAPPED);

  // Inject the migrated keyset
  Mock::VerifyAndClearExpectations(&platform);
  EXPECT_CALL(platform, FileExists(user->keyset_path))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform, ReadFile(user->keyset_path, _))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(migrated_keyset),
                          Return(true)));
  EXPECT_CALL(platform, FileExists(user->salt_path))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform, ReadFile(user->salt_path, _))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(user->user_salt),
                          Return(true)));
  EXPECT_CALL(tpm, Decrypt(_, _, _, _))
    .WillRepeatedly(Invoke(TpmPassthroughDecrypt));
  EXPECT_TRUE(homedirs.AreCredentialsValid(up));
}

TEST_F(MountTest, MountCryptohome) {
  // checks that cryptohome tries to mount successfully, and tests that the
  // tracked directories are created/replaced as expected
  InsertTestUsers(&kDefaultUsers[10], 1);
  Mount mount;
  NiceMock<MockTpm> tpm;
  mount.crypto()->set_tpm(&tpm);
  mount.set_shadow_root(kImageDir);
  mount.set_skel_source(kSkelDir);
  mount.set_use_tpm(false);
  set_policy(&mount, false, "", false);

  MockPlatform platform;
  mount.set_platform(&platform);
  mount.crypto()->set_platform(&platform);

  EXPECT_CALL(platform, DirectoryExists(kImageDir))
    .WillRepeatedly(Return(true));
  EXPECT_TRUE(DoMountInit(&mount));

  TestUser *user = &helper_.users[0];
  UsernamePasskey up(user->username, user->passkey);

  user->InjectUserPaths(&platform, chronos_uid_, shared_gid_, kDaemonGid);
  user->InjectKeyset(&platform);

  EXPECT_CALL(platform, AddEcryptfsAuthToken(_, _, _))
    .Times(2)
    .WillRepeatedly(Return(0));
  EXPECT_CALL(platform, ClearUserKeyring())
    .WillOnce(Return(0));

  EXPECT_CALL(platform, CreateDirectory(user->vault_mount_path))
    .WillRepeatedly(Return(true));

  EXPECT_CALL(platform, Mount(_, _, _, _))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform, Bind(_, _))
    .WillRepeatedly(Return(true));

  MountError error = MOUNT_ERROR_NONE;
  EXPECT_TRUE(mount.MountCryptohome(up, Mount::MountArgs(), &error));
}

TEST_F(MountTest, MountCryptohomeNoChange) {
  // checks that cryptohome doesn't by default re-save the cryptohome when mount
  Mount mount;
  NiceMock<MockTpm> tpm;
  NiceMock<MockPlatform> platform;
  Crypto crypto(&platform);
  mount.crypto()->set_tpm(&tpm);
  mount.crypto()->set_platform(&platform);
  mount.set_shadow_root(kImageDir);
  mount.set_skel_source(kSkelDir);
  mount.set_use_tpm(false);
  set_policy(&mount, false, "", false);

  mount.set_platform(&platform);

  EXPECT_CALL(platform, DirectoryExists(kImageDir))
    .WillRepeatedly(Return(true));
  EXPECT_TRUE(DoMountInit(&mount));

  InsertTestUsers(&kDefaultUsers[11], 1);
  TestUser* user = &helper_.users[0];
  UsernamePasskey up(user->username, user->passkey);

  user->InjectKeyset(&platform);
  VaultKeyset vault_keyset(&platform, &crypto);
  SerializedVaultKeyset serialized;
  MountError error;
  ASSERT_TRUE(mount.DecryptVaultKeyset(up, true, &vault_keyset, &serialized,
                                       &error));

  user->InjectUserPaths(&platform, chronos_uid_, shared_gid_, kDaemonGid);

  EXPECT_CALL(platform, CreateDirectory(user->vault_mount_path))
    .WillRepeatedly(Return(true));
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
  mount.crypto()->set_tpm(&tpm);
  mount.set_shadow_root(kImageDir);
  mount.set_skel_source(kSkelDir);
  mount.set_use_tpm(false);
  set_policy(&mount, false, "", false);

  NiceMock<MockPlatform> platform;
  mount.set_platform(&platform);
  mount.crypto()->set_platform(&platform);

  EXPECT_CALL(platform, DirectoryExists(kImageDir))
    .WillRepeatedly(Return(true));
  EXPECT_TRUE(DoMountInit(&mount));

  // Test user at index 12 hasn't been created
  InsertTestUsers(&kDefaultUsers[12], 1);
  TestUser* user = &helper_.users[0];
  UsernamePasskey up(user->username, user->passkey);

  user->InjectKeyset(&platform);

  // Doesn't exist.
  EXPECT_CALL(platform, DirectoryExists(user->vault_path))
    .WillOnce(Return(false));

  Mount::MountArgs mount_args;
  mount_args.create_if_missing = false;
  MountError error = MOUNT_ERROR_NONE;
  ASSERT_FALSE(mount.MountCryptohome(up, mount_args, &error));
  ASSERT_EQ(MOUNT_ERROR_USER_DOES_NOT_EXIST, error);

  // Now let it create the vault.
  // TODO(wad) Drop NiceMock and replace with InSequence EXPECT_CALL()s.
  // It will complain about creating tracked subdirs, but that is non-fatal.
  Mock::VerifyAndClearExpectations(&platform);
  user->InjectKeyset(&platform);

  EXPECT_CALL(platform,
      DirectoryExists(AnyOf(user->vault_path, user->user_vault_path)))
    .Times(2)
    .WillRepeatedly(Return(false));

  // Not legacy
  EXPECT_CALL(platform, FileExists(user->image_path))
    .WillRepeatedly(Return(false));

  EXPECT_CALL(platform, CreateDirectory(_))
    .WillRepeatedly(Return(true));
  chromeos::Blob creds;
  EXPECT_CALL(platform, WriteFile(user->keyset_path, _))
    .WillOnce(DoAll(SaveArg<1>(&creds), Return(true)));

  EXPECT_CALL(platform, Mount(_, _, _, _))
      .WillOnce(Return(true));
  EXPECT_CALL(platform, Bind(_, _))
      .Times(3)
      .WillRepeatedly(Return(true));

  // Fake successful mount to /home/chronos/user/*
  EXPECT_CALL(platform, FileExists(AnyOf(
                           StartsWith(user->legacy_user_mount_path),
                           StartsWith(user->vault_mount_path))))
    .WillRepeatedly(Return(true));

  mount_args.create_if_missing = true;
  error = MOUNT_ERROR_NONE;
  ASSERT_TRUE(mount.MountCryptohome(up, mount_args, &error));
  ASSERT_EQ(MOUNT_ERROR_NONE, error);
}

TEST_F(MountTest, UserActivityTimestampUpdated) {
  // checks that user activity timestamp is updated during Mount() and
  // periodically while mounted, other Keyset fields remains the same
  Mount mount;
  NiceMock<MockTpm> tpm;
  mount.crypto()->set_tpm(&tpm);
  mount.set_shadow_root(kImageDir);
  mount.set_skel_source(kSkelDir);
  mount.set_use_tpm(false);
  set_policy(&mount, false, "", false);

  NiceMock<MockPlatform> platform;
  mount.set_platform(&platform);
  mount.crypto()->set_platform(&platform);

  EXPECT_CALL(platform, DirectoryExists(kImageDir))
    .WillRepeatedly(Return(true));
  EXPECT_TRUE(DoMountInit(&mount));

  InsertTestUsers(&kDefaultUsers[9], 1);
  TestUser* user = &helper_.users[0];
  UsernamePasskey up(user->username, user->passkey);

  user->InjectKeyset(&platform);
  user->InjectUserPaths(&platform, chronos_uid_, shared_gid_, kDaemonGid);
  // Mount()
  MountError error;
  EXPECT_CALL(platform, Mount(_, _, _, _))
      .WillOnce(Return(true));
  EXPECT_CALL(platform, Bind(_, _))
      .Times(3)
      .WillRepeatedly(Return(true));
  ASSERT_TRUE(mount.MountCryptohome(up, Mount::MountArgs(), &error));

  // Update the timestamp. Normally it is called in MountTaskMount::Run() in
  // background but here in the test we must call it manually.
  static const int kMagicTimestamp = 123;
  chromeos::Blob updated_keyset;
  EXPECT_CALL(platform, WriteFile(user->keyset_path, _))
    .WillRepeatedly(DoAll(SaveArg<1>(&updated_keyset), Return(true)));
  EXPECT_CALL(platform, GetCurrentTime())
      .WillOnce(Return(base::Time::FromInternalValue(kMagicTimestamp)));
  mount.UpdateCurrentUserActivityTimestamp(0);
  SerializedVaultKeyset serialized1;
  ASSERT_TRUE(serialized1.ParseFromArray(
      static_cast<const unsigned char*>(&updated_keyset[0]),
      updated_keyset.size()));

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
  ASSERT_TRUE(serialized2.ParseFromArray(
      static_cast<const unsigned char*>(&updated_keyset[0]),
      updated_keyset.size()));
  ASSERT_TRUE(serialized2.has_last_activity_timestamp());
  EXPECT_EQ(kMagicTimestamp2, serialized2.last_activity_timestamp());

  // Update timestamp again, after user is unmounted. User's activity
  // timestamp must not change this.
  mount.UpdateCurrentUserActivityTimestamp(0);
  SerializedVaultKeyset serialized3;
  ASSERT_TRUE(serialized3.ParseFromArray(
      static_cast<const unsigned char*>(&updated_keyset[0]),
      updated_keyset.size()));
  ASSERT_TRUE(serialized3.has_last_activity_timestamp());
  EXPECT_EQ(serialized3.has_last_activity_timestamp(),
            serialized2.has_last_activity_timestamp());
}

TEST_F(MountTest, MountForUserOrderingTest) {
  // Checks that mounts made with MountForUser/BindForUser are undone in the
  // right order.
  Mount mount;
  NiceMock<MockTpm> tpm;
  NiceMock<MockPlatform> platform;
  mount.set_platform(&platform);

  mount.crypto()->set_tpm(&tpm);
  mount.crypto()->set_platform(&platform);
  mount.set_shadow_root(kImageDir);
  mount.set_skel_source(kSkelDir);
  mount.set_use_tpm(false);

  EXPECT_CALL(platform, DirectoryExists(kImageDir))
    .WillRepeatedly(Return(true));
  EXPECT_TRUE(DoMountInit(&mount));
  UserSession session;
  Crypto crypto(&platform);
  SecureBlob salt;
  salt.assign('A', 16);
  session.Init(salt);
  UsernamePasskey up("username", SecureBlob("password", 8));
  EXPECT_TRUE(session.SetUser(up));


  std::string src = "/src";
  std::string dest0 = "/dest/foo";
  std::string dest1 = "/dest/bar";
  std::string dest2 = "/dest/baz";
  {
    InSequence sequence;
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
}


// Test setup that initially has no cryptohomes.
const TestUserInfo kNoUsers[] = {
  {"user0@invalid.domain", "zero", false},
  {"user1@invalid.domain", "odin", false},
  {"user2@invalid.domain", "dwaa", false},
  {"owner@invalid.domain", "1234", false},
};
const int kNoUserCount = arraysize(kNoUsers);

// Test setup that initially has a cryptohome for the owner only.
const TestUserInfo kOwnerOnlyUsers[] = {
  {"user0@invalid.domain", "zero", false},
  {"user1@invalid.domain", "odin", false},
  {"user2@invalid.domain", "dwaa", false},
  {"owner@invalid.domain", "1234", true},
};
const int kOwnerOnlyUserCount = arraysize(kOwnerOnlyUsers);

// Test setup that initially has cryptohomes for all users.
const TestUserInfo kAlternateUsers[] = {
  {"user0@invalid.domain", "zero", true},
  {"user1@invalid.domain", "odin", true},
  {"user2@invalid.domain", "dwaa", true},
  {"owner@invalid.domain", "1234", true},
};
const int kAlternateUserCount = arraysize(kAlternateUsers);

class AltImageTest : public MountTest {
 public:
  AltImageTest() { }

  void SetUp(const TestUserInfo *users, int user_count) {
    // Set up fresh users.
    MountTest::SetUp();
    InsertTestUsers(users, user_count);

    // Initialize Mount object.
    mount_.set_platform(&platform_);
    mount_.crypto()->set_tpm(&tpm_);
    mount_.crypto()->set_platform(&platform_);
    mount_.homedirs()->set_platform(&platform_);
    mount_.set_shadow_root(kImageDir);
    mount_.set_use_tpm(false);
    set_policy(&mount_, false, "", false);

    EXPECT_CALL(platform_, DirectoryExists(kImageDir))
      .WillRepeatedly(Return(true));
    EXPECT_TRUE(DoMountInit(&mount_));
  }

  bool StoreSerializedKeyset(const SerializedVaultKeyset& serialized,
                             TestUser *user) {
    user->credentials.resize(serialized.ByteSize());
    serialized.SerializeWithCachedSizesToArray(
        static_cast<google::protobuf::uint8*>(&user->credentials[0]));
    return true;
  }

  // Set the user with specified |key_file| old.
  bool SetUserTimestamp(TestUser* user, base::Time timestamp) {
    SerializedVaultKeyset serialized;
    if (!LoadSerializedKeyset(user->credentials, &serialized)) {
      LOG(ERROR) << "Failed to parse keyset for "
                 << user->username;
      return false;
    }
    serialized.set_last_activity_timestamp(timestamp.ToInternalValue());
    bool ok = StoreSerializedKeyset(serialized, user);
    if (!ok) {
      LOG(ERROR) << "Failed to serialize new timestamp'd keyset for "
                 << user->username;
    }
    return ok;
  }

  void PrepareHomedirs(bool populate_cache,
                       bool populate_gcache,
                       bool inject_keyset,
                       const std::vector<int>* delete_vaults,
                       const std::vector<int>* mounted_vaults) {
    bool populate_vaults = (vaults_.size() == 0);
    // const string contents = "some encrypted contents";
    for (int user = 0; user != static_cast<int>(helper_.users.size()); user++) {
      // Let their Cache dirs be filled with some data.
      // Guarded to keep this function reusable.
      if (populate_vaults) {
        EXPECT_CALL(platform_,
            DirectoryExists(StartsWith(helper_.users[user].base_path)))
          .WillRepeatedly(Return(true));
        vaults_.push_back(helper_.users[user].base_path);
      }
      bool delete_user = false;
      if (delete_vaults && delete_vaults->size() != 0) {
        if (std::find(delete_vaults->begin(), delete_vaults->end(), user) !=
            delete_vaults->end())
          delete_user = true;
      }
      bool mounted_user = false;
      if (mounted_vaults && mounted_vaults->size() != 0) {
        if (std::find(mounted_vaults->begin(), mounted_vaults->end(), user) !=
            mounted_vaults->end())
          mounted_user = true;
      }

      // <vault>/user/Cache
      {
        InSequence two_tests;
        if (populate_cache && !mounted_user) {
          InSequence s;
          // Create a contents to be deleted.
          MockFileEnumerator* files = new MockFileEnumerator();
          EXPECT_CALL(platform_,
              GetFileEnumerator(StringPrintf("%s/%s",
                                  helper_.users[user].user_vault_path.c_str(),
                                  kCacheDir),
                                false, _))
            .WillOnce(Return(files));
          EXPECT_CALL(*files, Next())
            .WillOnce(Return("MOCK/cached_file"));
          EXPECT_CALL(platform_, DeleteFile("MOCK/cached_file", _))
            .WillOnce(Return(true));
          EXPECT_CALL(*files, Next())
            .WillOnce(Return("MOCK/cached_subdir"));
          EXPECT_CALL(platform_, DeleteFile("MOCK/cached_subdir", true))
            .WillOnce(Return(true));
          EXPECT_CALL(*files, Next())
            .WillRepeatedly(Return(""));
        }
        if (populate_gcache && !mounted_user) {
          InSequence s;
          // Create a contents to be deleted.
          MockFileEnumerator* files = new MockFileEnumerator();
          EXPECT_CALL(platform_,
              GetFileEnumerator(StartsWith(
                                  StringPrintf("%s/%s",
                                    helper_.users[user].user_vault_path.c_str(),
                                    kGCacheDir)),
                                false, _))
            .WillOnce(Return(files));
          EXPECT_CALL(*files, Next())
            .WillOnce(Return("MOCK/gcached_file"));
          EXPECT_CALL(platform_, DeleteFile("MOCK/gcached_file", _))
            .WillOnce(Return(true));
          EXPECT_CALL(*files, Next())
            .WillOnce(Return("MOCK/gcached_subdir"));
          EXPECT_CALL(platform_, DeleteFile("MOCK/gcached_subdir", true))
            .WillOnce(Return(true));
          EXPECT_CALL(*files, Next())
            .WillRepeatedly(Return(""));
        }
      }
      // After Cache & GCache are depleted. Users are deleted. To do so cleanly,
      // their keysets timestamps are read into an in-memory.
      if (inject_keyset && !mounted_user) {
        helper_.users[user].InjectKeyset(&platform_);
      }

      if (delete_user) {
        EXPECT_CALL(platform_,
            DeleteFile(helper_.users[user].base_path, true))
          .WillOnce(Return(true));
      }
    }
  }

  std::vector<std::string> vaults_;
 protected:
  Mount mount_;
  NiceMock<MockTpm> tpm_;
  MockPlatform platform_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AltImageTest);
};

// Checks DoAutomaticFreeDiskSpaceControl() to act in different situations when
// free disk space is low.
class DoAutomaticFreeDiskSpaceControlTest : public AltImageTest {
 public:
  DoAutomaticFreeDiskSpaceControlTest() { }

  void SetUp() {
    AltImageTest::SetUp(kAlternateUsers, kAlternateUserCount);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DoAutomaticFreeDiskSpaceControlTest);
};

TEST_F(DoAutomaticFreeDiskSpaceControlTest, NoCacheCleanup) {
  // Pretend we have lots of free space.
  EXPECT_CALL(platform_, AmountOfFreeDiskSpace(_))
      .WillRepeatedly(Return(kMinFreeSpace + 1));
  EXPECT_FALSE(mount_.DoAutomaticFreeDiskSpaceControl());
}

TEST_F(DoAutomaticFreeDiskSpaceControlTest, OnlyCacheCleanup) {
  PrepareHomedirs(true, false, false, NULL, NULL);

  // Let Mount know how many vaults there are.
  EXPECT_CALL(platform_, EnumerateDirectoryEntries(kImageDir, false, _))
    .WillRepeatedly(DoAll(SetArgumentPointee<2>(vaults_), Return(true)));
  // Don't say any cryptohomes are mounted
  EXPECT_CALL(platform_, IsDirectoryMountedWith(_, _))
    .WillRepeatedly(Return(false));

  // Pretend we have lack of free space, but only once so only
  // Caches should be cleaned and not GCache.
  EXPECT_CALL(platform_, AmountOfFreeDiskSpace(_))
      .WillOnce(Return(kMinFreeSpace - 1))
      .WillRepeatedly(Return(kEnoughFreeSpace));
  EXPECT_TRUE(mount_.DoAutomaticFreeDiskSpaceControl());
}

TEST_F(DoAutomaticFreeDiskSpaceControlTest, CacheAndGCacheCleanup) {
  PrepareHomedirs(true, true, true, NULL, NULL);

  // Let Mount know how many vaults there are.
  EXPECT_CALL(platform_, EnumerateDirectoryEntries(kImageDir, false, _))
    .WillRepeatedly(DoAll(SetArgumentPointee<2>(vaults_), Return(true)));
  // Don't say any cryptohomes are mounted
  EXPECT_CALL(platform_, IsDirectoryMountedWith(_, _))
    .WillRepeatedly(Return(false));

  // Now pretend we have lack of free space which results in cache and gcache
  // removal.  No Owner is set, though, so no vaults will be deleted.
  EXPECT_CALL(platform_, AmountOfFreeDiskSpace(_))
      .WillRepeatedly(Return(kMinFreeSpace - 1));
  EXPECT_TRUE(mount_.DoAutomaticFreeDiskSpaceControl());
}

TEST_F(DoAutomaticFreeDiskSpaceControlTest, OldUsersCleanupNoTimestamp) {
  // Removes old (except owner and the current one, if any) even if
  // users had no oldest activity timestamp.

  // Verify that user timestamp cache must be not initialized by now.
  UserOldestActivityTimestampCache* user_timestamp =
      mount_.user_timestamp_cache();
  EXPECT_FALSE(user_timestamp->initialized());

  // Make sure no users actually deleted as we didn't put
  // user timestamps, all users must remain.
  PrepareHomedirs(true, true, true, NULL, NULL);

  // Setting owner so that old user may be deleted.
  set_policy(&mount_, true, helper_.users[3].username, false);

  // Let Mount know how many vaults there are.
  EXPECT_CALL(platform_, EnumerateDirectoryEntries(kImageDir, false, _))
    .WillRepeatedly(DoAll(SetArgumentPointee<2>(vaults_), Return(true)));

  // Don't say any cryptohomes are mounted
  EXPECT_CALL(platform_, IsDirectoryMountedWith(_, _))
    .WillRepeatedly(Return(false));

  // Now pretend we have lack of free space.
  EXPECT_CALL(platform_, AmountOfFreeDiskSpace(_))
      .WillOnce(Return(kMinFreeSpace - 1))
      .WillRepeatedly(Return(kEnoughFreeSpace - 1));
  EXPECT_TRUE(mount_.DoAutomaticFreeDiskSpaceControl());

  // Verify that user timestamp cache must be initialized by now.
  EXPECT_TRUE(user_timestamp->initialized());

  // Simulate the user[0] have been updated but not old enough.
  user_timestamp->UpdateExistingUser(
      FilePath(helper_.users[0].base_path),
      base::Time::Now() - kOldUserLastActivityTime / 2);

  // Make sure no users actually deleted. Because the only
  // timestamp we put is not old enough.
  PrepareHomedirs(true, true, true, NULL, NULL);

  // Now pretend we have lack of free space.
  EXPECT_CALL(platform_, AmountOfFreeDiskSpace(_))
      .WillOnce(Return(kMinFreeSpace - 1))
      .WillRepeatedly(Return(kEnoughFreeSpace - 1));
  EXPECT_TRUE(mount_.DoAutomaticFreeDiskSpaceControl());

  // Verify that user timestamp cache must be initialized.
  EXPECT_TRUE(user_timestamp->initialized());

  // Simulate the user[0] have been updated old enough.
  user_timestamp->UpdateExistingUser(
      FilePath(helper_.users[0].base_path),
      base::Time::Now() - kOldUserLastActivityTime);

  // Make sure no users actually deleted. Because the only
  // timestamp we put is not old enough.
  std::vector<int> expect_deletion;
  expect_deletion.push_back(0);
  expect_deletion.push_back(1);
  expect_deletion.push_back(2);
  PrepareHomedirs(true, true, true, &expect_deletion, NULL);

  // Now pretend we have lack of free space.
  EXPECT_CALL(platform_, AmountOfFreeDiskSpace(_))
      .WillOnce(Return(kMinFreeSpace - 1))
      .WillRepeatedly(Return(kEnoughFreeSpace - 1));
  EXPECT_TRUE(mount_.DoAutomaticFreeDiskSpaceControl());
}

TEST_F(DoAutomaticFreeDiskSpaceControlTest, CleanUpOneOldUser) {
  // Remove old users, oldest first. Stops removing when disk space is enough.

  // Setting owner so that old user may be deleted.
  set_policy(&mount_, true, helper_.users[3].username, false);

  // Update cached users with following timestamps:
  // user[0] is old, user[1] is up to date, user[2] still have no timestamp,
  // user[3] is very old, but it is an owner.
  SetUserTimestamp(&helper_.users[0],
                   base::Time::Now() - kOldUserLastActivityTime);
  SetUserTimestamp(&helper_.users[1], base::Time::Now());
  SetUserTimestamp(&helper_.users[3],
                   base::Time::Now() - kOldUserLastActivityTime * 2);

  // Now pretend we have lack of free space 3 times.
  // So at 1st Caches are deleted, then GCache tmps are deleted
  // and then 1 oldest user is deleted.
  //
  // User[2] should be deleted because we have not updated its
  // timestamp (so it does not have one) and 1st user is old, so 2nd
  // user is older.
  std::vector<int> expect_deletion;
  expect_deletion.push_back(2);
  PrepareHomedirs(true, true, true, &expect_deletion, NULL);

  // Let Mount know how many vaults there are.
  EXPECT_CALL(platform_, EnumerateDirectoryEntries(kImageDir, false, _))
    .WillRepeatedly(DoAll(SetArgumentPointee<2>(vaults_), Return(true)));

  // Don't say any cryptohomes are mounted
  EXPECT_CALL(platform_, IsDirectoryMountedWith(_, _))
    .WillRepeatedly(Return(false));

  EXPECT_CALL(platform_, AmountOfFreeDiskSpace(_))
      .WillOnce(Return(kMinFreeSpace - 1))
      .WillOnce(Return(kMinFreeSpace - 1))
      .WillOnce(Return(kEnoughFreeSpace - 1))
      .WillRepeatedly(Return(kEnoughFreeSpace));
  EXPECT_TRUE(mount_.DoAutomaticFreeDiskSpaceControl());
}


TEST_F(DoAutomaticFreeDiskSpaceControlTest, CleanUpMultipleOldUsers) {
  // Remove old users, oldest first. Stops removing when disk space is enough.

  // Setting owner so that old user may be deleted.
  set_policy(&mount_, true, helper_.users[3].username, false);

  // Update cached users with following timestamps:
  // user[0] is old, user[1] is up to date, user[2] still have no timestamp,
  // user[3] is very old, but it is an owner.
  SetUserTimestamp(&helper_.users[0],
                   base::Time::Now() - kOldUserLastActivityTime);
  SetUserTimestamp(&helper_.users[1], base::Time::Now());
  SetUserTimestamp(&helper_.users[3],
                   base::Time::Now() - kOldUserLastActivityTime * 2);

  // Now pretend we have lack of free space 3 times.
  // So at 1st Caches are deleted, then GCache tmps are deleted
  // and then 1 oldest user is deleted.
  std::vector<int> expect_deletion;
  expect_deletion.push_back(0);
  expect_deletion.push_back(2);
  PrepareHomedirs(true, true, true, &expect_deletion, NULL);

  // Let Mount know how many vaults there are.
  EXPECT_CALL(platform_, EnumerateDirectoryEntries(kImageDir, false, _))
    .WillRepeatedly(DoAll(SetArgumentPointee<2>(vaults_), Return(true)));

  // Don't say any cryptohomes are mounted
  EXPECT_CALL(platform_, IsDirectoryMountedWith(_, _))
    .WillRepeatedly(Return(false));

  // Now pretend we have lack of free space at all times.
  // User[0] should be deleted because it is oldest now.
  // User[1] should not be deleted because it is up to date.
  // User[2] should be deleted because we have not updated its
  // timestamp (so it does not have one) and 1st user is old, so 2nd
  // user is older.
  EXPECT_CALL(platform_, AmountOfFreeDiskSpace(_))
      .WillOnce(Return(kMinFreeSpace - 1))
      .WillRepeatedly(Return(kEnoughFreeSpace - 1));
  EXPECT_TRUE(mount_.DoAutomaticFreeDiskSpaceControl());
}

TEST_F(DoAutomaticFreeDiskSpaceControlTest, OldUsersCleanupWithRestart) {
  // Cryptohomed may restart for some reason and continue nuking users
  // as if not restarted. Scenario is same as in test OldUsersCleanup.

  // Update cached users with following timestamps:
  // user[0] is old, user[1] is up to date, user[2] still have no timestamp,
  // user[3] is very old, but it is an owner.
  SetUserTimestamp(&helper_.users[0],
                   base::Time::Now() - kOldUserLastActivityTime);
  SetUserTimestamp(&helper_.users[1], base::Time::Now());
  SetUserTimestamp(&helper_.users[3],
                   base::Time::Now() - kOldUserLastActivityTime * 2);

  // Setting owner so that old user may be deleted.
  set_policy(&mount_, true, helper_.users[3].username, false);

  // Now pretend we have lack of free space 3 times.
  // user[0] is old, user[1] is up to date, user[2] still have no timestamp,
  // user[3] is very old, but it is an owner.
  // We expect that only user[2] will be removed.
  std::vector<int> expect_deletion;
  expect_deletion.push_back(2);
  PrepareHomedirs(true, true, true, &expect_deletion, NULL);

  // Let Mount know how many vaults there are.
  EXPECT_CALL(platform_, EnumerateDirectoryEntries(kImageDir, false, _))
    .WillRepeatedly(DoAll(SetArgumentPointee<2>(vaults_), Return(true)));

  // Don't say any cryptohomes are mounted
  EXPECT_CALL(platform_, IsDirectoryMountedWith(_, _))
    .WillRepeatedly(Return(false));

  EXPECT_CALL(platform_, AmountOfFreeDiskSpace(_))
      .WillOnce(Return(kMinFreeSpace - 1))
      .WillOnce(Return(kMinFreeSpace - 1))
      .WillOnce(Return(kEnoughFreeSpace - 1))
      .WillRepeatedly(Return(kEnoughFreeSpace));
  EXPECT_TRUE(mount_.DoAutomaticFreeDiskSpaceControl());

  // Forget about mount_ instance as if it has crashed.
  // Simulate cryptohome restart. Create new Mount instance.
  Mock::VerifyAndClearExpectations(&platform_);
  // Ensure that users_[2] is "gone".
  helper_.users.erase(helper_.users.begin() + 2);
  vaults_.clear();
  expect_deletion.clear();

  Mount mount2;
  mount2.set_platform(&platform_);
  mount2.crypto()->set_tpm(&tpm_);
  mount2.crypto()->set_platform(&platform_);
  mount2.homedirs()->set_platform(&platform_);
  mount2.set_shadow_root(kImageDir);
  mount2.set_use_tpm(false);

  EXPECT_CALL(platform_, DirectoryExists(kImageDir))
    .WillRepeatedly(Return(true));
  EXPECT_TRUE(DoMountInit(&mount2));

  // Expect only users_[0] to be removed since it is now
  // the oldest.
  expect_deletion.push_back(0);
  PrepareHomedirs(true, true, true, &expect_deletion, NULL);

  // Setting owner so that old user may be deleted.
  set_policy(&mount2, true, helper_.users[2].username, false);

  // Let Mount know how many vaults there are.
  EXPECT_CALL(platform_, EnumerateDirectoryEntries(kImageDir, false, _))
    .WillRepeatedly(DoAll(SetArgumentPointee<2>(vaults_), Return(true)));

  // Don't say any cryptohomes are mounted
  EXPECT_CALL(platform_, IsDirectoryMountedWith(_, _))
    .WillRepeatedly(Return(false));

  EXPECT_CALL(platform_, IsDirectoryMounted("/home/chronos/user"))
    .WillRepeatedly(Return(false));

  // Now pretend we have lack of free space at all times.
  EXPECT_CALL(platform_, AmountOfFreeDiskSpace(_))
      .WillOnce(Return(kMinFreeSpace - 1))
      .WillRepeatedly(Return(kEnoughFreeSpace - 1));
  EXPECT_TRUE(mount2.DoAutomaticFreeDiskSpaceControl());
}

TEST_F(DoAutomaticFreeDiskSpaceControlTest, OldUsersCleanupEphemeral) {
  // When ephemeral users are enabled, all users except owner should be removed.
  set_policy(&mount_, true, helper_.users[3].username, true);

  // Expect all accounts but the owner to be deleted.
  std::vector<int> expect_deletion;
  expect_deletion.push_back(0);
  expect_deletion.push_back(1);
  expect_deletion.push_back(2);
  PrepareHomedirs(false, false, false, &expect_deletion, NULL);

  // TODO(wad) test hash clean up here.
  std::vector<std::string> empty;
  EXPECT_CALL(platform_, EnumerateDirectoryEntries(
      chromeos::cryptohome::home::GetUserPathPrefix().value(), false, _))
    .WillRepeatedly(DoAll(SetArgumentPointee<2>(empty), Return(true)));
  EXPECT_CALL(platform_, EnumerateDirectoryEntries(
      chromeos::cryptohome::home::GetRootPathPrefix().value(), false, _))
    .WillRepeatedly(DoAll(SetArgumentPointee<2>(empty), Return(true)));

  // Let Mount know how many vaults there are.
  EXPECT_CALL(platform_, EnumerateDirectoryEntries(kImageDir, false, _))
    .WillRepeatedly(DoAll(SetArgumentPointee<2>(vaults_), Return(true)));

  // Don't say any cryptohomes are mounted
  EXPECT_CALL(platform_, IsDirectoryMountedWith(_, _))
    .WillRepeatedly(Return(false));

  // Pretend we have lack of free space.
  EXPECT_CALL(platform_, AmountOfFreeDiskSpace(_))
      .WillOnce(Return(kMinFreeSpace - 1));
  EXPECT_TRUE(mount_.DoAutomaticFreeDiskSpaceControl());
}

TEST_F(DoAutomaticFreeDiskSpaceControlTest, OldUsersCleanupNoOwnerSet) {
  // No users deleted when no owner known (set) and not in enterprise mode.
  PrepareHomedirs(true, true, true, NULL, NULL);

  // Update cached users with artificial timestamp: user[0] is old,
  // Other users still have no timestamp so we consider them even older.
  ASSERT_TRUE(SetUserTimestamp(
      &helper_.users[0], base::Time::Now() - kOldUserLastActivityTime));

  // Let Mount know how many vaults there are.
  EXPECT_CALL(platform_, EnumerateDirectoryEntries(kImageDir, false, _))
    .WillRepeatedly(DoAll(SetArgumentPointee<2>(vaults_), Return(true)));

  // Don't say any cryptohomes are mounted
  EXPECT_CALL(platform_, IsDirectoryMountedWith(_, _))
    .WillRepeatedly(Return(false));

  // Now pretend we have lack of free space at all times - to delete all users.
  // No users will be removed with no owner/enterprise, however.
  EXPECT_CALL(platform_, AmountOfFreeDiskSpace(_))
      .WillOnce(Return(kMinFreeSpace - 1))
      .WillRepeatedly(Return(kEnoughFreeSpace - 1));
  EXPECT_TRUE(mount_.DoAutomaticFreeDiskSpaceControl());
}

TEST_F(DoAutomaticFreeDiskSpaceControlTest, OldUsersCleanupEnterprise) {
  // All users must be deleted because they are either old or with no
  // timestamp. Owner is not counted because we are in enterprise
  // mode.
  set_policy(&mount_, true, "", false);  // enterprise-owned.
  mount_.set_enterprise_owned(true);

  // Update cached users with artificial timestamp: user[0] is old,
  // Other users still have no timestamp so we consider them even older.
  ASSERT_TRUE(SetUserTimestamp(
      &helper_.users[0], base::Time::Now() - kOldUserLastActivityTime));

  // Expect all users to be deleted.
  std::vector<int> expected_deletions;
  for (size_t u = 0; u < helper_.users.size(); ++u)
    expected_deletions.push_back(u);
  PrepareHomedirs(true, true, true, &expected_deletions, NULL);

  // Let Mount know how many vaults there are.
  EXPECT_CALL(platform_, EnumerateDirectoryEntries(kImageDir, false, _))
    .WillRepeatedly(DoAll(SetArgumentPointee<2>(vaults_), Return(true)));

  // Don't say any cryptohomes are mounted
  EXPECT_CALL(platform_, IsDirectoryMountedWith(_, _))
    .WillRepeatedly(Return(false));

  EXPECT_CALL(platform_, AmountOfFreeDiskSpace(_))
      .WillOnce(Return(kMinFreeSpace - 1))
      .WillRepeatedly(Return(kMinFreeSpace - 1));
  // Now we expect the user dirs to get destroyed.
  EXPECT_TRUE(mount_.DoAutomaticFreeDiskSpaceControl());
}

TEST_F(DoAutomaticFreeDiskSpaceControlTest, OldUsersCleanupWhenMounted) {
  // Do not remove currently mounted user and do remove it when unmounted.

  // Setting owner (user[3]) so that old user may be deleted.
  set_policy(&mount_, true, helper_.users[3].username, false);

  // Set all users old should one of them have a timestamp.
  mount_.set_old_user_last_activity_time(base::TimeDelta::FromMicroseconds(0));
  SetUserTimestamp(&helper_.users[3],
                   base::Time::Now() - kOldUserLastActivityTime);

  TestUser *user = &helper_.users[0];
  UsernamePasskey up(user->username, user->passkey);

  // Mount() user[0].
  MountError error;
  EXPECT_CALL(platform_, DirectoryExists(user->vault_path))
    .WillRepeatedly(Return(true));

  user->InjectUserPaths(&platform_, chronos_uid_, shared_gid_, kDaemonGid);
  user->InjectKeyset(&platform_);

  EXPECT_CALL(platform_, AddEcryptfsAuthToken(_, _, _))
    .Times(2)
    .WillRepeatedly(Return(0));
  EXPECT_CALL(platform_, ClearUserKeyring())
    .WillOnce(Return(0));

  EXPECT_CALL(platform_, CreateDirectory(user->vault_mount_path))
    .WillRepeatedly(Return(true));

  EXPECT_CALL(platform_, Mount(_, _, _, _))
      .WillOnce(Return(true));
  EXPECT_CALL(platform_, Bind(_, _))
      .Times(3)
      .WillRepeatedly(Return(true));
  ASSERT_TRUE(mount_.MountCryptohome(
      up, Mount::MountArgs(), &error));

  EXPECT_CALL(platform_, GetCurrentTime())
   .WillRepeatedly(Return(base::Time::Now()));
  EXPECT_CALL(platform_, WriteFile(user->keyset_path, _))
   .WillRepeatedly(DoAll(SaveArg<1>(&user->credentials), Return(true)));

  // Update current user timestamp.
  // Normally it is done in MountTaskMount::Run() in background.
  mount_.UpdateCurrentUserActivityTimestamp(0);

  std::vector<int> expected_deletions;
  // 0 should be mounted and 3 should be the owner.
  expected_deletions.push_back(1);
  expected_deletions.push_back(2);
  std::vector<int> mounted;
  mounted.push_back(0);
  PrepareHomedirs(true, true, true, &expected_deletions, &mounted);

  // Let Mount know how many vaults there are.
  EXPECT_CALL(platform_, EnumerateDirectoryEntries(kImageDir, false, _))
    .WillRepeatedly(DoAll(SetArgumentPointee<2>(vaults_), Return(true)));

  // Now pretend we have lack of free space.
  EXPECT_CALL(platform_, AmountOfFreeDiskSpace(_))
      .WillOnce(Return(kMinFreeSpace - 1))
      .WillRepeatedly(Return(kEnoughFreeSpace - 1));
  // Note that user_[0] is mounted.
  EXPECT_CALL(platform_, IsDirectoryMountedWith(_, Not(user->vault_path)))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(platform_, IsDirectoryMountedWith(_, user->vault_path))
    .WillRepeatedly(Return(true));
  EXPECT_TRUE(mount_.DoAutomaticFreeDiskSpaceControl());

  EXPECT_CALL(platform_, ClearUserKeyring())
    .WillRepeatedly(Return(0));

  // Now unmount the user. So it (user[0]) should be cached and may be
  // deleted next when it becomes old.
  EXPECT_CALL(platform_, Unmount(_, _, _))
      .Times(4)
      .WillRepeatedly(Return(true));
  mount_.UnmountCryptohome();

  // Get rid of users.
  Mock::VerifyAndClearExpectations(&platform_);

  helper_.InjectSystemSalt(&platform_, kImageSaltFile);

  // Ensure that users_[1,2] are "gone".
  helper_.users.erase(helper_.users.begin() + 1, helper_.users.begin() + 3);
  vaults_.clear();
  expected_deletions.clear();

  // Expect that user 0 will be deleted this time around.
  expected_deletions.push_back(0);
  PrepareHomedirs(true, true, true, &expected_deletions, NULL);

  // Let Mount know how many vaults there are.
  EXPECT_CALL(platform_, EnumerateDirectoryEntries(kImageDir, false, _))
    .WillRepeatedly(DoAll(SetArgumentPointee<2>(vaults_), Return(true)));

  // Don't say any cryptohomes are mounted
  EXPECT_CALL(platform_, IsDirectoryMountedWith(_, _))
    .WillRepeatedly(Return(false));

  // Now pretend we have lack of free space.
  EXPECT_CALL(platform_, AmountOfFreeDiskSpace(_))
      .WillOnce(Return(kMinFreeSpace - 1))
      .WillRepeatedly(Return(kEnoughFreeSpace - 1));
  EXPECT_CALL(platform_, IsDirectoryMountedWith(_, _))
      .WillRepeatedly(Return(false));
  EXPECT_TRUE(mount_.DoAutomaticFreeDiskSpaceControl());
}

class EphemeralNoUserSystemTest : public AltImageTest {
 public:
  EphemeralNoUserSystemTest() { }

  void SetUp() {
    AltImageTest::SetUp(kNoUsers, kNoUserCount);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(EphemeralNoUserSystemTest);
};

TEST_F(EphemeralNoUserSystemTest, OwnerUnknownMountCreateTest) {
  // Checks that when a device is not enterprise enrolled and does not have a
  // known owner, a regular vault is created and mounted.
  set_policy(&mount_, false, "", true);

  // Setup a NiceMock to not need to mock the entire creation flow.
  NiceMock<MockPlatform> platform;
  mount_.set_platform(&platform);
  mount_.crypto()->set_platform(&platform);
  mount_.homedirs()->set_platform(&platform);

  TestUser *user = &helper_.users[0];
  UsernamePasskey up(user->username, user->passkey);

  EXPECT_CALL(platform, FileExists(_))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform, FileExists(user->image_path))
    .WillRepeatedly(Return(false));
  EXPECT_CALL(platform, DirectoryExists(user->vault_path))
    .WillRepeatedly(Return(false));
  EXPECT_CALL(platform, CreateDirectory(_))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform, WriteFile(user->keyset_path, _))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform, ReadFile(user->keyset_path, _))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(user->credentials),
                          Return(true)));
  EXPECT_CALL(platform, DirectoryExists(StartsWith(user->user_vault_path)))
    .WillRepeatedly(Return(true));

  EXPECT_CALL(platform, Mount(_, _, kEphemeralMountType, _))
      .Times(0);
  EXPECT_CALL(platform, Mount(_, _, _, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(platform, Bind(_, _))
      .WillRepeatedly(Return(true));

  Mount::MountArgs mount_args;
  mount_args.create_if_missing = true;
  MountError error;
  ASSERT_TRUE(mount_.MountCryptohome(up,
                                     mount_args,
                                     &error));
}

TEST_F(EphemeralNoUserSystemTest, EnterpriseMountNoCreateTest) {
  // Checks that when a device is enterprise enrolled, a tmpfs cryptohome is
  // mounted and no regular vault is created.
  set_policy(&mount_, false, "", true);
  mount_.set_enterprise_owned(true);
  TestUser *user = &helper_.users[0];

  // Always removes non-owner cryptohomes.
  std::vector<std::string> empty;
  EXPECT_CALL(platform_, EnumerateDirectoryEntries(_, _, _))
    .WillRepeatedly(DoAll(SetArgumentPointee<2>(empty), Return(true)));

  EXPECT_CALL(platform_, GetFileEnumerator(_, _, _))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>()))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>()));

  EXPECT_CALL(platform_, DirectoryExists(_))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, Stat(_, _))
    .WillRepeatedly(Return(false));
  EXPECT_CALL(platform_, CreateDirectory(user->vault_path))
    .Times(0);
  EXPECT_CALL(platform_, CreateDirectory(_))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, SetOwnership(_, _, _))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, SetGroupAccessible(_, _, _))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, DeleteFile(_, _))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, FileExists(_))
    .WillRepeatedly(Return(true));

  // Make sure it's a tmpfs mount until we move to ephemeral key use.
  EXPECT_CALL(platform_, Mount(_, _, _, _))
      .Times(0);
  EXPECT_CALL(platform_, Mount(_, _, kEphemeralMountType, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, Bind(_, _))
      .WillRepeatedly(Return(true));

  Mount::MountArgs mount_args;
  mount_args.create_if_missing = true;
  MountError error;
  UsernamePasskey up(user->username, user->passkey);
  ASSERT_TRUE(mount_.MountCryptohome(up, mount_args, &error));
}

TEST_F(EphemeralNoUserSystemTest, OwnerUnknownMountEnsureEphemeralTest) {
  // Checks that when a device is not enterprise enrolled and does not have a
  // known owner, a mount request with the |ensure_ephemeral| flag set fails.
  set_policy(&mount_, false, "", false);
  TestUser *user = &helper_.users[0];

  EXPECT_CALL(platform_, Mount(_, _, _, _)).Times(0);

  Mount::MountArgs mount_args;
  mount_args.create_if_missing = true;
  mount_args.ensure_ephemeral = true;
  MountError error;
  UsernamePasskey up(user->username, user->passkey);
  ASSERT_FALSE(mount_.MountCryptohome(up, mount_args, &error));
  ASSERT_EQ(MOUNT_ERROR_FATAL, error);
}

TEST_F(EphemeralNoUserSystemTest, EnterpriseMountEnsureEphemeralTest) {
  // Checks that when a device is enterprise enrolled, a mount request with the
  // |ensure_ephemeral| flag set causes a tmpfs cryptohome to be mounted and no
  // regular vault to be created.
  set_policy(&mount_, true, "", false);
  mount_.set_enterprise_owned(true);
  TestUser *user = &helper_.users[0];

  // Always removes non-owner cryptohomes.
  std::vector<std::string> empty;
  EXPECT_CALL(platform_, EnumerateDirectoryEntries(_, _, _))
    .WillRepeatedly(DoAll(SetArgumentPointee<2>(empty), Return(true)));

  EXPECT_CALL(platform_, GetFileEnumerator(_, _, _))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>()))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>()));

  EXPECT_CALL(platform_, DirectoryExists(_))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, Stat(_, _))
    .WillRepeatedly(Return(false));
  EXPECT_CALL(platform_, CreateDirectory(user->vault_path))
    .Times(0);
  EXPECT_CALL(platform_, CreateDirectory(_))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, SetOwnership(_, _, _))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, SetGroupAccessible(_, _, _))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, DeleteFile(_, _))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, FileExists(_))
    .WillRepeatedly(Return(true));

  EXPECT_CALL(platform_, Mount(_, _, _, _))
      .Times(0);
  EXPECT_CALL(platform_, Mount(_, _, kEphemeralMountType, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, Bind(_, _))
      .WillRepeatedly(Return(true));

  Mount::MountArgs mount_args;
  mount_args.create_if_missing = true;
  mount_args.ensure_ephemeral = true;
  MountError error;
  UsernamePasskey up(user->username, user->passkey);
  ASSERT_TRUE(mount_.MountCryptohome(up, mount_args, &error));
}

class EphemeralOwnerOnlySystemTest : public AltImageTest {
 public:
  EphemeralOwnerOnlySystemTest() { }

  void SetUp() {
    AltImageTest::SetUp(kOwnerOnlyUsers, kOwnerOnlyUserCount);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(EphemeralOwnerOnlySystemTest);
};

TEST_F(EphemeralOwnerOnlySystemTest, MountNoCreateTest) {
  // Checks that when a device is not enterprise enrolled and has a known owner,
  // a tmpfs cryptohome is mounted and no regular vault is created.
  TestUser* owner = &helper_.users[3];
  TestUser* user = &helper_.users[0];
  set_policy(&mount_, true, owner->username, true);
  UsernamePasskey up(user->username, user->passkey);

  // Always removes non-owner cryptohomes.
  std::vector<std::string> owner_only;
  owner_only.push_back(owner->base_path);

  EXPECT_CALL(platform_, EnumerateDirectoryEntries(_, _, _))
    .WillRepeatedly(DoAll(SetArgumentPointee<2>(owner_only), Return(true)));

  EXPECT_CALL(platform_, GetFileEnumerator(_, _, _))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>()))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>()));

  EXPECT_CALL(platform_, DirectoryExists(_))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, Stat(_, _))
    .WillRepeatedly(Return(false));
  EXPECT_CALL(platform_, CreateDirectory(user->vault_path))
    .Times(0);
  EXPECT_CALL(platform_, CreateDirectory(_))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, SetOwnership(_, _, _))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, SetGroupAccessible(_, _, _))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, DeleteFile(_, _))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, FileExists(_))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, IsDirectoryMountedWith(_, _))
    .WillRepeatedly(Return(false));

  EXPECT_CALL(platform_, Mount(_, _, _, _))
      .Times(0);
  EXPECT_CALL(platform_, Mount(_, _, kEphemeralMountType, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, Bind(_, _))
      .WillRepeatedly(Return(true));

  Mount::MountArgs mount_args;
  mount_args.create_if_missing = true;
  MountError error;
  ASSERT_TRUE(mount_.MountCryptohome(up, mount_args, &error));
}

TEST_F(EphemeralOwnerOnlySystemTest, NonOwnerMountEnsureEphemeralTest) {
  // Checks that when a device is not enterprise enrolled and has a known owner,
  // a mount request for a non-owner user with the |ensure_ephemeral| flag set
  // causes a tmpfs cryptohome to be mounted and no regular vault to be created.
  TestUser* owner = &helper_.users[3];
  TestUser* user = &helper_.users[0];
  set_policy(&mount_, true, owner->username, false);
  UsernamePasskey up(user->username, user->passkey);

  // Always removes non-owner cryptohomes.
  std::vector<std::string> owner_only;
  owner_only.push_back(owner->base_path);

  EXPECT_CALL(platform_, EnumerateDirectoryEntries(_, _, _))
    .WillRepeatedly(DoAll(SetArgumentPointee<2>(owner_only), Return(true)));

  EXPECT_CALL(platform_, GetFileEnumerator(_, _, _))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>()))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>()));

  EXPECT_CALL(platform_, DirectoryExists(_))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, Stat(_, _))
    .WillRepeatedly(Return(false));
  EXPECT_CALL(platform_, CreateDirectory(user->vault_path))
    .Times(0);
  EXPECT_CALL(platform_, CreateDirectory(_))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, SetOwnership(_, _, _))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, SetGroupAccessible(_, _, _))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, DeleteFile(_, _))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, FileExists(_))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, IsDirectoryMountedWith(_, _))
    .WillRepeatedly(Return(false));

  EXPECT_CALL(platform_, Mount(_, _, _, _))
      .Times(0);
  EXPECT_CALL(platform_, Mount(_, _, kEphemeralMountType, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, Bind(_, _))
      .WillRepeatedly(Return(true));

  Mount::MountArgs mount_args;
  mount_args.create_if_missing = true;
  mount_args.ensure_ephemeral = true;
  MountError error;
  ASSERT_TRUE(mount_.MountCryptohome(up, mount_args, &error));
}

TEST_F(EphemeralOwnerOnlySystemTest, OwnerMountEnsureEphemeralTest) {
  // Checks that when a device is not enterprise enrolled and has a known owner,
  // a mount request for the owner with the |ensure_ephemeral| flag set fails.
  TestUser* owner = &helper_.users[3];
  set_policy(&mount_, true, owner->username, false);
  UsernamePasskey up(owner->username, owner->passkey);

  EXPECT_CALL(platform_, Mount(_, _, _, _)).Times(0);

  Mount::MountArgs mount_args;
  mount_args.create_if_missing = true;
  mount_args.ensure_ephemeral = true;
  MountError error;
  ASSERT_FALSE(mount_.MountCryptohome(up, mount_args, &error));
  ASSERT_EQ(MOUNT_ERROR_FATAL, error);
}

class EphemeralExistingUserSystemTest : public AltImageTest {
 public:
  EphemeralExistingUserSystemTest() { }

  void SetUp() {
    AltImageTest::SetUp(kAlternateUsers, kAlternateUserCount);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(EphemeralExistingUserSystemTest);
};

TEST_F(EphemeralExistingUserSystemTest, OwnerUnknownMountNoRemoveTest) {
  // Checks that when a device is not enterprise enrolled and does not have a
  // known owner, no stale cryptohomes are removed while mounting.
  set_policy(&mount_, false, "", true);
  TestUser* user = &helper_.users[0];

  // No c-homes will be removed.  The rest of the mocking just gets us to
  // Mount().
  std::vector<TestUser>::iterator it;
  for (it = helper_.users.begin(); it != helper_.users.end(); ++it)
    it->InjectUserPaths(&platform_, chronos_uid_, shared_gid_, kDaemonGid);

  std::vector<std::string> empty;
  EXPECT_CALL(platform_, EnumerateDirectoryEntries(_, _, _))
    .WillOnce(DoAll(SetArgumentPointee<2>(empty), Return(true)));

  EXPECT_CALL(platform_, DirectoryExists(_))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, Stat(_, _))
    .WillRepeatedly(Return(false));

  EXPECT_CALL(platform_, AddEcryptfsAuthToken(_, _, _))
    .Times(2)
    .WillRepeatedly(Return(0));
  EXPECT_CALL(platform_, ClearUserKeyring())
    .WillOnce(Return(0));

  EXPECT_CALL(platform_, CreateDirectory(user->vault_path))
    .Times(0);
  EXPECT_CALL(platform_, CreateDirectory(_))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, SetOwnership(_, _, _))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, SetPermissions(_, _))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, SetGroupAccessible(_, _, _))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, DeleteFile(_, _))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, FileExists(_))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, IsDirectoryMountedWith(_, _))
    .WillRepeatedly(Return(false));

  EXPECT_CALL(platform_, Mount(_, _, _, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, Mount(_, _, kEphemeralMountType, _))
      .Times(0);
  EXPECT_CALL(platform_, Bind(_, _))
      .WillRepeatedly(Return(true));

  Mount::MountArgs mount_args;
  mount_args.create_if_missing = true;
  MountError error;
  user->InjectKeyset(&platform_);
  UsernamePasskey up(user->username, user->passkey);
  ASSERT_TRUE(mount_.MountCryptohome(up, mount_args, &error));
}

TEST_F(EphemeralExistingUserSystemTest, EnterpriseMountRemoveTest) {
  // Checks that when a device is enterprise enrolled, all stale cryptohomes are
  // removed while mounting.
  set_policy(&mount_, false, "", true);
  mount_.set_enterprise_owned(true);
  TestUser* user = &helper_.users[0];
  UsernamePasskey up(user->username, user->passkey);

  std::vector<int> expect_deletion;
  expect_deletion.push_back(0);
  expect_deletion.push_back(1);
  expect_deletion.push_back(2);
  expect_deletion.push_back(3);
  PrepareHomedirs(false, false, true, &expect_deletion, NULL);

  // Let Mount know how many vaults there are.
  EXPECT_CALL(platform_, EnumerateDirectoryEntries(kImageDir, false, _))
    .WillRepeatedly(DoAll(SetArgumentPointee<2>(vaults_), Return(true)));
  // Don't say any cryptohomes are mounted
  EXPECT_CALL(platform_, IsDirectoryMountedWith(_, _))
    .WillRepeatedly(Return(false));
  std::vector<std::string> empty;
  EXPECT_CALL(platform_,
      EnumerateDirectoryEntries(AnyOf("/home/root/",
                                      "/home/user/"), _, _))
    .WillRepeatedly(DoAll(SetArgumentPointee<2>(empty), Return(true)));
  EXPECT_CALL(platform_,
      Stat(AnyOf("/home",
                 "/home/root",
                 chromeos::cryptohome::home::GetRootPath(
                     user->username).value(),
                 "/home/user",
                 chromeos::cryptohome::home::GetUserPath(
                     user->username).value()),
           _))
    .WillRepeatedly(Return(false));
  helper_.InjectEphemeralSkeleton(&platform_, kImageDir, false);
  user->InjectUserPaths(&platform_, chronos_uid_, shared_gid_, kDaemonGid);
  // Only expect the mounted user to "exist".
  EXPECT_CALL(platform_, DirectoryExists(StartsWith(user->user_mount_path)))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, CreateDirectory(_))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, SetOwnership(_, _, _))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, SetPermissions(_, _))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, SetGroupAccessible(_, _, _))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, GetFileEnumerator("/etc/skel", _, _))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>()))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>()));

  EXPECT_CALL(platform_, Mount(_, _, _, _))
      .Times(0);
  EXPECT_CALL(platform_, Mount(_, _, kEphemeralMountType, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, Bind(_, _))
      .WillRepeatedly(Return(true));

  Mount::MountArgs mount_args;
  mount_args.create_if_missing = true;
  MountError error;
  ASSERT_TRUE(mount_.MountCryptohome(up, mount_args, &error));
}

TEST_F(EphemeralExistingUserSystemTest, MountRemoveTest) {
  // Checks that when a device is not enterprise enrolled and has a known owner,
  // all non-owner cryptohomes are removed while mounting.
  TestUser* owner = &helper_.users[3];
  set_policy(&mount_, true, owner->username, true);
  TestUser* user = &helper_.users[0];
  UsernamePasskey up(user->username, user->passkey);

  std::vector<int> expect_deletion;
  expect_deletion.push_back(0); // the mounting user shouldn't use be persistent
  expect_deletion.push_back(1);
  expect_deletion.push_back(2);
  // Expect all users but the owner to be removed.
  PrepareHomedirs(false, false, true, &expect_deletion, NULL);

  // Let Mount know how many vaults there are.
  EXPECT_CALL(platform_, EnumerateDirectoryEntries(kImageDir, false, _))
    .WillRepeatedly(DoAll(SetArgumentPointee<2>(vaults_), Return(true)));
  // Don't say any cryptohomes are mounted
  EXPECT_CALL(platform_, IsDirectoryMountedWith(_, _))
    .WillRepeatedly(Return(false));
  std::vector<std::string> empty;
  EXPECT_CALL(platform_,
      EnumerateDirectoryEntries(AnyOf("/home/root/",
                                      "/home/user/"), _, _))
    .WillRepeatedly(DoAll(SetArgumentPointee<2>(empty), Return(true)));
  EXPECT_CALL(platform_,
      Stat(AnyOf("/home",
                 "/home/root",
                 chromeos::cryptohome::home::GetRootPath(
                   user->username).value(),
                 "/home/user",
                 chromeos::cryptohome::home::GetUserPath(
                   user->username).value()),
           _))
    .WillRepeatedly(Return(false));
  helper_.InjectEphemeralSkeleton(&platform_, kImageDir, false);
  user->InjectUserPaths(&platform_, chronos_uid_, shared_gid_, kDaemonGid);
  // Only expect the mounted user to "exist".
  EXPECT_CALL(platform_, DirectoryExists(StartsWith(user->user_mount_path)))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, CreateDirectory(_))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, SetOwnership(_, _, _))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, SetPermissions(_, _))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, SetGroupAccessible(_, _, _))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, GetFileEnumerator("/etc/skel", _, _))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>()))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>()));

  EXPECT_CALL(platform_, Mount(_, _, _, _))
      .Times(0);
  EXPECT_CALL(platform_, Mount(_, _, kEphemeralMountType, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, Bind(_, _))
      .WillRepeatedly(Return(true));

  Mount::MountArgs mount_args;
  mount_args.create_if_missing = true;
  MountError error;
  ASSERT_TRUE(mount_.MountCryptohome(up, mount_args, &error));
}

TEST_F(EphemeralExistingUserSystemTest, OwnerUnknownUnmountNoRemoveTest) {
  // Checks that when a device is not enterprise enrolled and does not have a
  // known owner, no stale cryptohomes are removed while unmounting.
  set_policy(&mount_, false, "", true);
  EXPECT_CALL(platform_, ClearUserKeyring())
    .WillOnce(Return(0));
  ASSERT_TRUE(mount_.UnmountCryptohome());
}

TEST_F(EphemeralExistingUserSystemTest, EnterpriseUnmountRemoveTest) {
  // Checks that when a device is enterprise enrolled, all stale cryptohomes are
  // removed while unmounting.
  set_policy(&mount_, false, "", true);
  mount_.set_enterprise_owned(true);

  std::vector<int> expect_deletion;
  expect_deletion.push_back(0);
  expect_deletion.push_back(1);
  expect_deletion.push_back(2);
  expect_deletion.push_back(3);
  PrepareHomedirs(false, false, false, &expect_deletion, NULL);

  // Let Mount know how many vaults there are.
  EXPECT_CALL(platform_, EnumerateDirectoryEntries(kImageDir, false, _))
    .WillRepeatedly(DoAll(SetArgumentPointee<2>(vaults_), Return(true)));

  // Don't say any cryptohomes are mounted
  EXPECT_CALL(platform_, IsDirectoryMountedWith(_, _))
    .WillRepeatedly(Return(false));
  std::vector<std::string> empty;
  EXPECT_CALL(platform_,
      EnumerateDirectoryEntries(AnyOf("/home/root/",
                                      "/home/user/"), _, _))
    .WillRepeatedly(DoAll(SetArgumentPointee<2>(empty), Return(true)));

  EXPECT_CALL(platform_, ClearUserKeyring())
    .WillOnce(Return(0));

  ASSERT_TRUE(mount_.UnmountCryptohome());
}

TEST_F(EphemeralExistingUserSystemTest, UnmountRemoveTest) {
  // Checks that when a device is not enterprise enrolled and has a known owner,
  // all stale cryptohomes are removed while unmounting.
  TestUser* owner = &helper_.users[3];
  set_policy(&mount_, true, owner->username, true);
  // All users but the owner.
  std::vector<int> expect_deletion;
  expect_deletion.push_back(0);
  expect_deletion.push_back(1);
  expect_deletion.push_back(2);
  PrepareHomedirs(false, false, false, &expect_deletion, NULL);

  // Let Mount know how many vaults there are.
  EXPECT_CALL(platform_, EnumerateDirectoryEntries(kImageDir, false, _))
    .WillRepeatedly(DoAll(SetArgumentPointee<2>(vaults_), Return(true)));

  // Don't say any cryptohomes are mounted
  EXPECT_CALL(platform_, IsDirectoryMountedWith(_, _))
    .WillRepeatedly(Return(false));
  std::vector<std::string> empty;
  EXPECT_CALL(platform_,
      EnumerateDirectoryEntries(AnyOf("/home/root/",
                                      "/home/user/"), _, _))
    .WillRepeatedly(DoAll(SetArgumentPointee<2>(empty), Return(true)));

  EXPECT_CALL(platform_, ClearUserKeyring())
    .WillOnce(Return(0));

  ASSERT_TRUE(mount_.UnmountCryptohome());
}

TEST_F(EphemeralExistingUserSystemTest, NonOwnerMountEnsureEphemeralTest) {
  // Checks that when a device is not enterprise enrolled and has a known owner,
  // a mount request for a non-owner user with the |ensure_ephemeral| flag set
  // causes a tmpfs cryptohome to be mounted, even if a regular vault exists for
  // the user.
  // Since ephemeral users aren't enabled, no vaults will be deleted.
  TestUser* owner = &helper_.users[3];
  set_policy(&mount_, true, owner->username, false);
  TestUser* user = &helper_.users[0];
  UsernamePasskey up(user->username, user->passkey);

  PrepareHomedirs(false, false, true, NULL, NULL);

  // Let Mount know how many vaults there are.
  EXPECT_CALL(platform_, EnumerateDirectoryEntries(kImageDir, false, _))
    .WillRepeatedly(DoAll(SetArgumentPointee<2>(vaults_), Return(true)));
  // Don't say any cryptohomes are mounted
  EXPECT_CALL(platform_, IsDirectoryMountedWith(_, _))
    .WillRepeatedly(Return(false));
  std::vector<std::string> empty;
  EXPECT_CALL(platform_,
      EnumerateDirectoryEntries(AnyOf("/home/root/",
                                      "/home/user/"), _, _))
    .WillRepeatedly(DoAll(SetArgumentPointee<2>(empty), Return(true)));
  EXPECT_CALL(platform_,
      Stat(AnyOf("/home",
                 "/home/root",
                 chromeos::cryptohome::home::GetRootPath(
                   user->username).value(),
                 "/home/user",
                 chromeos::cryptohome::home::GetUserPath(
                   user->username).value()),
           _))
    .WillRepeatedly(Return(false));
  // Only expect the mounted user to "exist".
  EXPECT_CALL(platform_, DirectoryExists(StartsWith(user->user_mount_path)))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, CreateDirectory(_))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, SetOwnership(_, _, _))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, SetPermissions(_, _))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, SetGroupAccessible(_, _, _))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, GetFileEnumerator("/etc/skel", _, _))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>()))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>()));
  EXPECT_CALL(platform_, FileExists(StartsWith("/home/chronos/user")))
    .WillRepeatedly(Return(true));

  helper_.InjectEphemeralSkeleton(&platform_, kImageDir, false);

  EXPECT_CALL(platform_, Mount(_, _, _, _))
      .Times(0);
  EXPECT_CALL(platform_, Mount(_, _, kEphemeralMountType, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, Bind(_, _))
      .WillRepeatedly(Return(true));

  Mount::MountArgs mount_args;
  mount_args.create_if_missing = true;
  mount_args.ensure_ephemeral = true;
  MountError error;
  ASSERT_TRUE(mount_.MountCryptohome(up, mount_args, &error));
}

TEST_F(EphemeralExistingUserSystemTest, EnterpriseMountEnsureEphemeralTest) {
  // Checks that when a device is enterprise enrolled, a mount request with the
  // |ensure_ephemeral| flag set causes a tmpfs cryptohome to be mounted, even
  // if a regular vault exists for the user.
  // Since ephemeral users aren't enabled, no vaults will be deleted.
  set_policy(&mount_, true, "", false);
  mount_.set_enterprise_owned(true);

  TestUser* user = &helper_.users[0];
  UsernamePasskey up(user->username, user->passkey);

  // Mounting user vault won't be deleted, but tmpfs mount should still be
  // used.
  PrepareHomedirs(false, false, true, NULL, NULL);

  // Let Mount know how many vaults there are.
  EXPECT_CALL(platform_, EnumerateDirectoryEntries(kImageDir, false, _))
    .WillRepeatedly(DoAll(SetArgumentPointee<2>(vaults_), Return(true)));
  // Don't say any cryptohomes are mounted
  EXPECT_CALL(platform_, IsDirectoryMountedWith(_, _))
    .WillRepeatedly(Return(false));
  std::vector<std::string> empty;
  EXPECT_CALL(platform_,
      EnumerateDirectoryEntries(AnyOf("/home/root/",
                                      "/home/user/"), _, _))
    .WillRepeatedly(DoAll(SetArgumentPointee<2>(empty), Return(true)));
  EXPECT_CALL(platform_,
      Stat(AnyOf("/home",
                 "/home/root",
                 chromeos::cryptohome::home::GetRootPath(
                   user->username).value(),
                 "/home/user",
                 chromeos::cryptohome::home::GetUserPath(
                   user->username).value()),
           _))
    .WillRepeatedly(Return(false));
  // Only expect the mounted user to "exist".
  EXPECT_CALL(platform_, DirectoryExists(StartsWith(user->user_mount_path)))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, CreateDirectory(_))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, SetOwnership(_, _, _))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, SetPermissions(_, _))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, SetGroupAccessible(_, _, _))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, GetFileEnumerator("/etc/skel", _, _))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>()))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>()));
  EXPECT_CALL(platform_, FileExists(StartsWith("/home/chronos/user")))
    .WillRepeatedly(Return(true));

  helper_.InjectEphemeralSkeleton(&platform_, kImageDir, false);

  EXPECT_CALL(platform_, Mount(_, _, _, _))
      .Times(0);
  EXPECT_CALL(platform_, Mount(_, _, kEphemeralMountType, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, Bind(_, _))
      .WillRepeatedly(Return(true));

  Mount::MountArgs mount_args;
  mount_args.create_if_missing = true;
  mount_args.ensure_ephemeral = true;
  MountError error;
  ASSERT_TRUE(mount_.MountCryptohome(up, mount_args, &error));
}
}  // namespace cryptohome
