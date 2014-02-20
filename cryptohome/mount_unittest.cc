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
#include "mock_homedirs.h"
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
using ::testing::EndsWith;
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
  scoped_refptr<Mount> mount = new Mount();
  NiceMock<MockTpm> tpm;
  mount->crypto()->set_tpm(&tpm);
  mount->set_shadow_root("/dev/null");
  mount->set_skel_source(kSkelDir);
  mount->set_use_tpm(false);
  MockPlatform platform;
  mount->set_platform(&platform);
  mount->crypto()->set_platform(&platform);
  set_policy(mount.get(), false, "", false);

  NiceMock<MockHomeDirs> mock_homedirs;
  mount->set_homedirs(&mock_homedirs);
  EXPECT_CALL(mock_homedirs, Init())
    .WillOnce(Return(true));

  SecureBlob passkey;
  cryptohome::Crypto::PasswordToPasskey(kDefaultUsers[0].password,
                                        helper_.system_salt, &passkey);
  UsernamePasskey up(kDefaultUsers[0].username, passkey);

  // shadow root creation should fail
  EXPECT_CALL(platform, DirectoryExists("/dev/null"))
    .WillOnce(Return(false));
  EXPECT_CALL(platform, CreateDirectory("/dev/null"))
    .WillOnce(Return(false));
  // salt creation failure because shadow_root is bogus
  EXPECT_CALL(platform, FileExists("/dev/null/salt"))
    .WillOnce(Return(false));
  EXPECT_CALL(platform, WriteFile("/dev/null/salt", _))
    .WillOnce(Return(false));
  EXPECT_CALL(platform, GetUserId("chronos", _, _))
    .WillOnce(DoAll(SetArgumentPointee<1>(1000), SetArgumentPointee<2>(1000),
                    Return(true)));
  EXPECT_CALL(platform, GetUserId("chaps", _, _))
    .WillOnce(DoAll(SetArgumentPointee<1>(1001), SetArgumentPointee<2>(1001),
                    Return(true)));
  EXPECT_CALL(platform, GetGroupId("chronos-access", _))
    .WillOnce(DoAll(SetArgumentPointee<1>(1002), Return(true)));
  EXPECT_FALSE(mount->Init());
  ASSERT_FALSE(mount->AreValid(up));
}

TEST_F(MountTest, CurrentCredentialsTest) {
  // create a Mount instance that points to a good shadow root, test that it
  // properly authenticates against the first key
  scoped_refptr<Mount> mount = new Mount();
  NiceMock<MockTpm> tpm;
  NiceMock<MockPlatform> platform;
  mount->crypto()->set_tpm(&tpm);
  mount->crypto()->set_platform(&platform);
  mount->set_shadow_root(kImageDir);
  mount->set_skel_source(kSkelDir);
  mount->set_use_tpm(false);
  set_policy(mount.get(), false, "", false);

  SecureBlob passkey;
  cryptohome::Crypto::PasswordToPasskey(kDefaultUsers[3].password,
                                        helper_.system_salt, &passkey);
  UsernamePasskey up(kDefaultUsers[3].username, passkey);

  helper_.InjectSystemSalt(&platform, kImageSaltFile);
  EXPECT_TRUE(mount->Init());

  NiceMock<MockUserSession> user_session;
  user_session.Init(SecureBlob());
  user_session.SetUser(up);
  mount->set_current_user(&user_session);

  EXPECT_CALL(user_session, CheckUser(_))
      .WillOnce(Return(true));
  EXPECT_CALL(user_session, Verify(_))
      .WillOnce(Return(true));

  ASSERT_TRUE(mount->AreValid(up));
}

TEST_F(MountTest, BadDecryptTest) {
  // create a Mount instance that points to a good shadow root, test that it
  // properly denies access with a bad passkey
  scoped_refptr<Mount> mount = new Mount();
  NiceMock<MockTpm> tpm;
  mount->crypto()->set_tpm(&tpm);
  mount->set_shadow_root(kImageDir);
  mount->set_skel_source(kSkelDir);
  mount->set_use_tpm(false);
  set_policy(mount.get(), false, "", false);

  SecureBlob passkey;
  cryptohome::Crypto::PasswordToPasskey("bogus", helper_.system_salt, &passkey);
  UsernamePasskey up(kDefaultUsers[4].username, passkey);

  EXPECT_TRUE(mount->Init());
  ASSERT_FALSE(mount->AreValid(up));
}

TEST_F(MountTest, MountCryptohomeNoPrivileges) {
  // Check that Mount only works if the mount permission is given.
  InsertTestUsers(&kDefaultUsers[10], 1);
  scoped_refptr<Mount> mount = new Mount();
  NiceMock<MockTpm> tpm;
  mount->crypto()->set_tpm(&tpm);
  mount->set_shadow_root(kImageDir);
  mount->set_skel_source(kSkelDir);
  mount->set_use_tpm(false);
  set_policy(mount.get(), false, "", false);

  MockPlatform platform;
  mount->set_platform(&platform);
  mount->crypto()->set_platform(&platform);

  NiceMock<MockHomeDirs> mock_homedirs;
  mount->set_homedirs(&mock_homedirs);
  EXPECT_CALL(mock_homedirs, Init())
    .WillOnce(Return(true));

  EXPECT_CALL(platform, SetMask(_))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform, DirectoryExists(kImageDir))
    .WillRepeatedly(Return(true));
  EXPECT_TRUE(DoMountInit(mount.get()));

  TestUser *user = &helper_.users[0];
  user->key_data.set_label("my key!");
  user->use_key_data = true;
  user->key_data.mutable_privileges()->set_mount(false);
  // Regenerate the serialized vault keyset.
  user->GenerateCredentials();
  UsernamePasskey up(user->username, user->passkey);
  // Let the legacy key iteration work here.

  user->InjectUserPaths(&platform, chronos_uid_, chronos_gid_, shared_gid_,
                        kDaemonGid);
  user->InjectKeyset(&platform, false);

  std::vector<int> key_indices;
  key_indices.push_back(0);
  EXPECT_CALL(mock_homedirs, GetVaultKeysets(user->obfuscated_username, _))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(key_indices),
                          Return(true)));

  EXPECT_CALL(platform, ClearUserKeyring())
    .WillOnce(Return(0));

  EXPECT_CALL(platform, CreateDirectory(user->vault_mount_path))
    .WillRepeatedly(Return(true));

  EXPECT_CALL(platform,
              CreateDirectory(mount->GetNewUserPath(user->username)))
    .WillRepeatedly(Return(true));

  MountError error = MOUNT_ERROR_NONE;
  EXPECT_FALSE(mount->MountCryptohome(up, Mount::MountArgs(), &error));
  EXPECT_EQ(MOUNT_ERROR_KEY_FAILURE, error);
}

TEST_F(MountTest, MountCryptohomeHasPrivileges) {
  // Check that Mount only works if the mount permission is given.
  InsertTestUsers(&kDefaultUsers[10], 1);
  scoped_refptr<Mount> mount = new Mount();
  NiceMock<MockTpm> tpm;
  mount->crypto()->set_tpm(&tpm);
  mount->set_shadow_root(kImageDir);
  mount->set_skel_source(kSkelDir);
  mount->set_use_tpm(false);
  set_policy(mount.get(), false, "", false);

  MockPlatform platform;
  mount->set_platform(&platform);
  mount->crypto()->set_platform(&platform);

  NiceMock<MockHomeDirs> mock_homedirs;
  mount->set_homedirs(&mock_homedirs);
  EXPECT_CALL(mock_homedirs, Init())
    .WillOnce(Return(true));

  EXPECT_CALL(platform, SetMask(_))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform, DirectoryExists(kImageDir))
    .WillRepeatedly(Return(true));
  EXPECT_TRUE(DoMountInit(mount.get()));

  TestUser *user = &helper_.users[0];
  user->key_data.set_label("my key!");
  user->use_key_data = true;
  user->key_data.mutable_privileges()->set_mount(true);
  // Regenerate the serialized vault keyset.
  user->GenerateCredentials();
  UsernamePasskey up(user->username, user->passkey);
  // Let the legacy key iteration work here.

  user->InjectUserPaths(&platform, chronos_uid_, chronos_gid_, shared_gid_,
                        kDaemonGid);
  user->InjectKeyset(&platform, false);

  std::vector<int> key_indices;
  key_indices.push_back(0);
  EXPECT_CALL(mock_homedirs, GetVaultKeysets(user->obfuscated_username, _))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(key_indices),
                          Return(true)));

  EXPECT_CALL(platform, AddEcryptfsAuthToken(_, _, _))
    .Times(2)
    .WillRepeatedly(Return(0));
  EXPECT_CALL(platform, ClearUserKeyring())
    .WillOnce(Return(0));

  EXPECT_CALL(platform, CreateDirectory(user->vault_mount_path))
    .WillRepeatedly(Return(true));

  EXPECT_CALL(platform,
              CreateDirectory(mount->GetNewUserPath(user->username)))
    .WillRepeatedly(Return(true));

  EXPECT_CALL(platform, IsDirectoryMounted(user->vault_mount_path))
    .WillOnce(Return(false));
  // user exists, so there'll be no skel copy after.
  EXPECT_CALL(platform, Mount(_, _, _, _))
    .WillRepeatedly(Return(true));
  // Only one mount, so the legacy mount point is used.
  EXPECT_CALL(platform, IsDirectoryMounted("/home/chronos/user"))
    .WillOnce(Return(false));
  EXPECT_CALL(platform, Bind(_, _))
    .WillRepeatedly(Return(true));

  MountError error = MOUNT_ERROR_NONE;
  ASSERT_TRUE(mount->MountCryptohome(up, Mount::MountArgs(), &error));

  EXPECT_CALL(platform, Unmount(_, _, _))
      .Times(5)
      .WillRepeatedly(Return(true));

  // Unmount here to avoid the scoped Mount doing it implicitly.
  EXPECT_CALL(platform, GetCurrentTime())
      .WillOnce(Return(base::Time::Now()));
  EXPECT_CALL(platform, WriteFile(user->keyset_path, _))
      .WillOnce(Return(true));
  EXPECT_CALL(platform, ClearUserKeyring())
    .WillOnce(Return(0));
  EXPECT_TRUE(mount->UnmountCryptohome());
}


// A fixture for testing chaps directory checks.
class ChapsDirectoryTest : public ::testing::Test {
 public:
  ChapsDirectoryTest()
      : kBaseDir("/base_chaps_dir"),
        kSaltFile("/base_chaps_dir/auth_data_salt"),
        kDatabaseDir("/base_chaps_dir/database"),
        kDatabaseFile("/base_chaps_dir/database/file"),
        kLegacyDir("/legacy"),
        kRootUID(0), kRootGID(0), kChapsUID(1), kSharedGID(2),
        mount_(new Mount()) {
    mount_->crypto()->set_platform(&platform_);
    mount_->set_platform(&platform_);
    mount_->chaps_user_ = kChapsUID;
    mount_->default_access_group_ = kSharedGID;
    // By default, set stats to the expected values.
    InitStat(&base_stat_, 040750, kChapsUID, kSharedGID);
    InitStat(&salt_stat_, 0600, kRootUID, kRootGID);
    InitStat(&database_dir_stat_, 040750, kChapsUID, kSharedGID);
    InitStat(&database_file_stat_, 0640, kChapsUID, kSharedGID);
  }

  virtual ~ChapsDirectoryTest() {}

  void SetupFakeChapsDirectory() {
    // Configure the base directory.
    EXPECT_CALL(platform_, DirectoryExists(kBaseDir))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(platform_, Stat(kBaseDir, _))
        .WillRepeatedly(DoAll(SetArgumentPointee<1>(base_stat_), Return(true)));

    // Configure a fake enumerator.
    MockFileEnumerator* enumerator = platform_.mock_enumerator();
    enumerator->entries_.push_back(kBaseDir);
    enumerator->entries_.push_back(kSaltFile);
    enumerator->entries_.push_back(kDatabaseDir);
    enumerator->entries_.push_back(kDatabaseFile);
    FileEnumerator::FindInfo find_info1 = {base_stat_, kBaseDir};
    FileEnumerator::FindInfo find_info2 = {salt_stat_, kSaltFile};
    FileEnumerator::FindInfo find_info3 = {database_dir_stat_, kDatabaseDir};
    FileEnumerator::FindInfo find_info4 = {database_file_stat_, kDatabaseFile};
    enumerator->find_info_.push_back(find_info1);
    enumerator->find_info_.push_back(find_info2);
    enumerator->find_info_.push_back(find_info3);
    enumerator->find_info_.push_back(find_info4);
  }

  bool RunCheck() {
    return mount_->CheckChapsDirectory(kBaseDir, kLegacyDir);
  }

 protected:
  const std::string kBaseDir;
  const std::string kSaltFile;
  const std::string kDatabaseDir;
  const std::string kDatabaseFile;
  const std::string kLegacyDir;
  const uid_t kRootUID;
  const gid_t kRootGID;
  const uid_t kChapsUID;
  const gid_t kSharedGID;

  struct stat base_stat_;
  struct stat salt_stat_;
  struct stat database_dir_stat_;
  struct stat database_file_stat_;

  scoped_refptr<Mount> mount_;
  NiceMock<MockPlatform> platform_;

 private:
  void InitStat(struct stat* s, mode_t mode, uid_t uid, gid_t gid) {
    memset(s, 0, sizeof(struct stat));
    s->st_mode = mode;
    s->st_uid = uid;
    s->st_gid = gid;
  }

  DISALLOW_COPY_AND_ASSIGN(ChapsDirectoryTest);
};

TEST_F(ChapsDirectoryTest, DirectoryOK) {
  SetupFakeChapsDirectory();
  ASSERT_TRUE(RunCheck());
}

TEST_F(ChapsDirectoryTest, DirectoryDoesNotExist) {
  // Specify directory does not exist.
  EXPECT_CALL(platform_, DirectoryExists(kBaseDir))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(platform_, DirectoryExists(kLegacyDir))
      .WillRepeatedly(Return(false));
  // Expect basic setup.
  EXPECT_CALL(platform_, CreateDirectory(kBaseDir))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, SetPermissions(kBaseDir, 0750))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, SetOwnership(kBaseDir, kChapsUID, kSharedGID))
      .WillRepeatedly(Return(true));
  ASSERT_TRUE(RunCheck());
}

TEST_F(ChapsDirectoryTest, CreateFailure) {
  // Specify directory does not exist.
  EXPECT_CALL(platform_, DirectoryExists(kBaseDir))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(platform_, DirectoryExists(kLegacyDir))
      .WillRepeatedly(Return(false));
  // Expect basic setup but fail.
  EXPECT_CALL(platform_, CreateDirectory(kBaseDir))
      .WillRepeatedly(Return(false));
  ASSERT_FALSE(RunCheck());
}

TEST_F(ChapsDirectoryTest, FixBadPerms) {
  // Specify some bad perms.
  base_stat_.st_mode = 040700;
  salt_stat_.st_mode = 0640;
  database_dir_stat_.st_mode = 040755;
  database_file_stat_.st_mode = 0666;
  SetupFakeChapsDirectory();
  // Expect corrections.
  EXPECT_CALL(platform_, SetPermissions(kBaseDir, 0750))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, SetPermissions(kSaltFile, 0600))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, SetPermissions(kDatabaseDir, 0750))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, SetPermissions(kDatabaseFile, 0640))
      .WillRepeatedly(Return(true));
  ASSERT_TRUE(RunCheck());
}

TEST_F(ChapsDirectoryTest, FixBadOwnership) {
  // Specify bad ownership.
  base_stat_.st_uid = kRootUID;
  salt_stat_.st_gid = kChapsUID;
  database_dir_stat_.st_gid = kChapsUID;
  database_file_stat_.st_uid = kSharedGID;
  SetupFakeChapsDirectory();
  // Expect corrections.
  EXPECT_CALL(platform_, SetOwnership(kBaseDir, kChapsUID, kSharedGID))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, SetOwnership(kSaltFile, kRootUID, kRootGID))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, SetOwnership(kDatabaseDir, kChapsUID, kSharedGID))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, SetOwnership(kDatabaseFile, kChapsUID, kSharedGID))
      .WillRepeatedly(Return(true));
  ASSERT_TRUE(RunCheck());
}

TEST_F(ChapsDirectoryTest, FixBadPermsFailure) {
  // Specify some bad perms.
  base_stat_.st_mode = 040700;
  SetupFakeChapsDirectory();
  // Expect corrections but fail to apply.
  EXPECT_CALL(platform_, SetPermissions(_, _))
      .WillRepeatedly(Return(false));
  ASSERT_FALSE(RunCheck());
}

TEST_F(ChapsDirectoryTest, FixBadOwnershipFailure) {
  // Specify bad ownership.
  base_stat_.st_uid = kRootUID;
  SetupFakeChapsDirectory();
  // Expect corrections but fail to apply.
  EXPECT_CALL(platform_, SetOwnership(_, _, _))
      .WillRepeatedly(Return(false));
  ASSERT_FALSE(RunCheck());
}

TEST_F(MountTest, CheckChapsDirectoryMigration) {
  scoped_refptr<Mount> mount = new Mount();
  NiceMock<MockPlatform> platform;
  NiceMock<MockTpm> tpm;
  mount->crypto()->set_tpm(&tpm);
  mount->crypto()->set_platform(&platform);
  mount->set_use_tpm(false);
  mount->set_platform(&platform);

  // Configure stub methods.
  EXPECT_CALL(platform, Copy(_, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(platform, DeleteFile(_, _))
      .WillRepeatedly(Return(true));

  // Stubs which will trigger the migration code path.
  EXPECT_CALL(platform, DirectoryExists("/fake"))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(platform, DirectoryExists("/fake_legacy"))
      .WillRepeatedly(Return(true));

  // Configure stat for the base directory.
  struct stat base_stat = {0};
  base_stat.st_mode = 040123;
  base_stat.st_uid = 1;
  base_stat.st_gid = 2;
  EXPECT_CALL(platform, Stat(_, _))
      .WillRepeatedly(DoAll(SetArgumentPointee<1>(base_stat), Return(true)));

  // Configure a fake enumerator.
  MockFileEnumerator* enumerator = platform.mock_enumerator();
  enumerator->entries_.push_back("/fake_legacy/test_file1");
  enumerator->entries_.push_back("test_file2");
  FileEnumerator::FindInfo find_info1 = {{0}, "/fake_legacy/test_file1"};
  find_info1.stat.st_mode = 0555;
  find_info1.stat.st_uid = 3;
  find_info1.stat.st_gid = 4;
  FileEnumerator::FindInfo find_info2 = {{0}, "test_file2"};
  find_info2.stat.st_mode = 0777;
  find_info2.stat.st_uid = 5;
  find_info2.stat.st_gid = 6;
  enumerator->find_info_.push_back(find_info1);
  enumerator->find_info_.push_back(find_info2);

  // These expectations will ensure the ownership and permissions are being
  // correctly applied after the directory has been moved.
  EXPECT_CALL(platform, SetOwnership("/fake/test_file1", 3, 4)).Times(1);
  EXPECT_CALL(platform, SetPermissions("/fake/test_file1", 0555)).Times(1);
  EXPECT_CALL(platform, SetOwnership("/fake/test_file2", 5, 6)).Times(1);
  EXPECT_CALL(platform, SetPermissions("/fake/test_file2", 0777)).Times(1);
  EXPECT_CALL(platform, SetOwnership("/fake", 1, 2)).Times(1);
  EXPECT_CALL(platform, SetPermissions("/fake", 0123)).Times(1);

  EXPECT_TRUE(mount->CheckChapsDirectory("/fake", "/fake_legacy"));
}

TEST_F(MountTest, CreateCryptohomeTest) {
  InsertTestUsers(&kDefaultUsers[5], 1);
  // creates a cryptohome and tests credentials
  scoped_refptr<Mount> mount = new Mount();
  HomeDirs homedirs;
  NiceMock<MockTpm> tpm;
  NiceMock<MockPlatform> platform;
  set_policy(mount.get(), false, "", false);
  mount->crypto()->set_tpm(&tpm);
  mount->set_shadow_root(kImageDir);
  mount->set_skel_source(kSkelDir);
  mount->set_use_tpm(false);
  homedirs.set_shadow_root(kImageDir);
  homedirs.set_platform(&platform);
  homedirs.set_crypto(mount->crypto());
  mount->crypto()->set_platform(&platform);
  mount->set_platform(&platform);

  NiceMock<MockHomeDirs> mock_homedirs;
  mount->set_homedirs(&mock_homedirs);
  EXPECT_CALL(mock_homedirs, Init())
    .WillOnce(Return(true));

  TestUser *user = &helper_.users[0];
  UsernamePasskey up(user->username, user->passkey);

  EXPECT_TRUE(DoMountInit(mount.get()));
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
    .Times(7)
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform,
      CreateDirectory(
        AnyOf("/home/chronos",
              mount->GetNewUserPath(user->username))))
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

  ASSERT_TRUE(mount->EnsureCryptohome(up, &created));
  ASSERT_TRUE(created);
  ASSERT_NE(creds.size(), 0);
  ASSERT_FALSE(mount->AreValid(up));
  {
    InSequence s;
    MockFileEnumerator* files = new MockFileEnumerator();
    EXPECT_CALL(platform, GetFileEnumerator(user->base_path, false, _))
      .WillOnce(Return(files));
     // Single key.
    EXPECT_CALL(*files, Next())
      .WillOnce(Return(user->keyset_path));
    EXPECT_CALL(*files, Next())
      .WillRepeatedly(Return(""));
  }

  EXPECT_CALL(platform, ReadFile(user->keyset_path, _))
    .WillOnce(DoAll(SetArgumentPointee<1>(creds), Return(true)));
  ASSERT_TRUE(homedirs.AreCredentialsValid(up));
}

TEST_F(MountTest, GoodReDecryptTest) {
  InsertTestUsers(&kDefaultUsers[6], 1);
  // create a Mount instance that points to a good shadow root, test that it
  // properly re-authenticates against the first key
  HomeDirs homedirs;
  scoped_refptr<Mount> mount = new Mount();
  MockTpm tpm;
  NiceMock<MockPlatform> platform;
  Crypto crypto(&platform);
  crypto.set_use_tpm(true);
  crypto.set_tpm(&tpm);
  mount->set_use_tpm(true);
  mount->set_shadow_root(kImageDir);
  mount->set_skel_source(kSkelDir);
  mount->set_platform(&platform);
  NiceMock<MockHomeDirs> mock_homedirs;
  mount->set_homedirs(&mock_homedirs);
  EXPECT_CALL(mock_homedirs, Init())
    .WillOnce(Return(true));

  mount->crypto()->set_tpm(&tpm);
  mount->crypto()->set_use_tpm(true);
  mount->crypto()->set_platform(&platform);
  set_policy(mount.get(), false, "", false);
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
  EXPECT_TRUE(mount->Init());
  EXPECT_TRUE(homedirs.Init());

  // Load the pre-generated keyset
  std::string key_path = mount->GetUserLegacyKeyFileForUser(
      up.GetObfuscatedUsername(helper_.system_salt), 0);
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
  VaultKeyset vault_keyset;
  vault_keyset.Initialize(&platform, &crypto);
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
  int key_index = 0;

  std::vector<int> key_indices;
  key_indices.push_back(0);
  EXPECT_CALL(mock_homedirs, GetVaultKeysets(user->obfuscated_username, _))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(key_indices),
                          Return(true)));

  EXPECT_TRUE(mount->DecryptVaultKeyset(up, true, &vault_keyset, &serialized,
                                        &key_index, &error));
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

    MockFileEnumerator* files = new MockFileEnumerator();
    EXPECT_CALL(platform, GetFileEnumerator(user->base_path, false, _))
      .WillOnce(Return(files));
     // Single key.
  {
    InSequence s;
    EXPECT_CALL(*files, Next())
      .WillOnce(Return(user->keyset_path));
    EXPECT_CALL(*files, Next())
      .WillOnce(Return(""));
  }

  EXPECT_TRUE(homedirs.AreCredentialsValid(up));
}

TEST_F(MountTest, MountCryptohome) {
  // checks that cryptohome tries to mount successfully, and tests that the
  // tracked directories are created/replaced as expected
  InsertTestUsers(&kDefaultUsers[10], 1);
  scoped_refptr<Mount> mount = new Mount();
  NiceMock<MockTpm> tpm;
  mount->crypto()->set_tpm(&tpm);
  mount->set_shadow_root(kImageDir);
  mount->set_skel_source(kSkelDir);
  mount->set_use_tpm(false);
  set_policy(mount.get(), false, "", false);

  MockPlatform platform;
  mount->set_platform(&platform);
  mount->crypto()->set_platform(&platform);

  NiceMock<MockHomeDirs> mock_homedirs;
  mount->set_homedirs(&mock_homedirs);
  EXPECT_CALL(mock_homedirs, Init())
    .WillOnce(Return(true));

  EXPECT_CALL(platform, DirectoryExists(kImageDir))
    .WillRepeatedly(Return(true));
  EXPECT_TRUE(DoMountInit(mount.get()));

  TestUser *user = &helper_.users[0];
  UsernamePasskey up(user->username, user->passkey);

  user->InjectUserPaths(&platform, chronos_uid_, chronos_gid_, shared_gid_,
                        kDaemonGid);
  user->InjectKeyset(&platform, false);

  std::vector<int> key_indices;
  key_indices.push_back(0);
  EXPECT_CALL(mock_homedirs, GetVaultKeysets(user->obfuscated_username, _))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(key_indices),
                          Return(true)));

  EXPECT_CALL(platform, AddEcryptfsAuthToken(_, _, _))
    .Times(2)
    .WillRepeatedly(Return(0));
  EXPECT_CALL(platform, ClearUserKeyring())
    .WillOnce(Return(0));

  EXPECT_CALL(platform, CreateDirectory(user->vault_mount_path))
    .WillRepeatedly(Return(true));

  EXPECT_CALL(platform,
              CreateDirectory(mount->GetNewUserPath(user->username)))
    .WillRepeatedly(Return(true));

  EXPECT_CALL(platform, IsDirectoryMounted(user->vault_mount_path))
    .WillOnce(Return(false));
  // user exists, so there'll be no skel copy after.
  EXPECT_CALL(platform, Mount(_, _, _, _))
    .WillRepeatedly(Return(true));
  // Only one mount, so the legacy mount point is used.
  EXPECT_CALL(platform, IsDirectoryMounted("/home/chronos/user"))
    .WillOnce(Return(false));
  EXPECT_CALL(platform, Bind(_, _))
    .WillRepeatedly(Return(true));

  MountError error = MOUNT_ERROR_NONE;
  EXPECT_TRUE(mount->MountCryptohome(up, Mount::MountArgs(), &error));
}

TEST_F(MountTest, MountCryptohomeNoChange) {
  // checks that cryptohome doesn't by default re-save the cryptohome when mount
  scoped_refptr<Mount> mount = new Mount();
  NiceMock<MockTpm> tpm;
  NiceMock<MockPlatform> platform;
  Crypto crypto(&platform);
  mount->crypto()->set_tpm(&tpm);
  mount->crypto()->set_platform(&platform);
  mount->set_shadow_root(kImageDir);
  mount->set_skel_source(kSkelDir);
  mount->set_use_tpm(false);
  set_policy(mount.get(), false, "", false);

  mount->set_platform(&platform);

  NiceMock<MockHomeDirs> mock_homedirs;
  mount->set_homedirs(&mock_homedirs);
  EXPECT_CALL(mock_homedirs, Init())
    .WillOnce(Return(true));

  EXPECT_CALL(platform, DirectoryExists(kImageDir))
    .WillRepeatedly(Return(true));
  EXPECT_TRUE(DoMountInit(mount.get()));

  InsertTestUsers(&kDefaultUsers[11], 1);
  TestUser* user = &helper_.users[0];
  UsernamePasskey up(user->username, user->passkey);

  user->InjectKeyset(&platform, false);
  VaultKeyset vault_keyset;
  vault_keyset.Initialize(&platform, &crypto);
  SerializedVaultKeyset serialized;
  MountError error;
  int key_index = -1;
  std::vector<int> key_indices;
  key_indices.push_back(0);
  EXPECT_CALL(mock_homedirs, GetVaultKeysets(user->obfuscated_username, _))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(key_indices),
                          Return(true)));

  ASSERT_TRUE(mount->DecryptVaultKeyset(up, true, &vault_keyset, &serialized,
                                        &key_index, &error));
  EXPECT_EQ(key_index, key_indices[0]);

  user->InjectUserPaths(&platform, chronos_uid_, chronos_gid_,
                        shared_gid_, kDaemonGid);

  EXPECT_CALL(platform, CreateDirectory(user->vault_mount_path))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform,
              CreateDirectory(mount->GetNewUserPath(user->username)))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform, Mount(_, _, _, _))
      .WillOnce(Return(true));
  EXPECT_CALL(platform, Bind(_, _))
      .Times(4)
      .WillRepeatedly(Return(true));

  ASSERT_TRUE(mount->MountCryptohome(up, Mount::MountArgs(), &error));

  SerializedVaultKeyset new_serialized;
  ASSERT_TRUE(mount->DecryptVaultKeyset(up, true, &vault_keyset,
                                        &new_serialized, &key_index, &error));

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
  scoped_refptr<Mount> mount = new Mount();
  NiceMock<MockTpm> tpm;
  mount->crypto()->set_tpm(&tpm);
  mount->set_shadow_root(kImageDir);
  mount->set_skel_source(kSkelDir);
  mount->set_use_tpm(false);
  set_policy(mount.get(), false, "", false);

  NiceMock<MockPlatform> platform;
  mount->set_platform(&platform);
  mount->crypto()->set_platform(&platform);

  NiceMock<MockHomeDirs> mock_homedirs;
  mount->set_homedirs(&mock_homedirs);
  EXPECT_CALL(mock_homedirs, Init())
    .WillOnce(Return(true));

  EXPECT_CALL(platform, DirectoryExists(kImageDir))
    .WillRepeatedly(Return(true));
  EXPECT_TRUE(DoMountInit(mount.get()));

  // Test user at index 12 hasn't been created
  InsertTestUsers(&kDefaultUsers[12], 1);
  TestUser* user = &helper_.users[0];
  UsernamePasskey up(user->username, user->passkey);

  user->InjectKeyset(&platform, false);

  std::vector<int> key_indices;
  key_indices.push_back(0);
  EXPECT_CALL(mock_homedirs, GetVaultKeysets(user->obfuscated_username, _))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(key_indices),
                          Return(true)));

  // Doesn't exist.
  EXPECT_CALL(platform, DirectoryExists(user->vault_path))
    .WillOnce(Return(false));

  Mount::MountArgs mount_args;
  mount_args.create_if_missing = false;
  MountError error = MOUNT_ERROR_NONE;
  ASSERT_FALSE(mount->MountCryptohome(up, mount_args, &error));
  ASSERT_EQ(MOUNT_ERROR_USER_DOES_NOT_EXIST, error);

  // Now let it create the vault.
  // TODO(wad) Drop NiceMock and replace with InSequence EXPECT_CALL()s.
  // It will complain about creating tracked subdirs, but that is non-fatal.
  Mock::VerifyAndClearExpectations(&platform);
  user->InjectKeyset(&platform, false);

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

  EXPECT_CALL(platform, IsDirectoryMounted(user->vault_mount_path))
    .WillOnce(Return(false));  // mount precondition
  EXPECT_CALL(platform, Mount(_, _, _, _))
      .WillOnce(Return(true));
  EXPECT_CALL(platform, IsDirectoryMounted("/home/chronos/user"))
    .WillOnce(Return(false));  // bind precondition for first mount
  EXPECT_CALL(platform, Bind(_, _))
      .Times(4)
      .WillRepeatedly(Return(true));

  // Fake successful mount to /home/chronos/user/*
  EXPECT_CALL(platform, FileExists(AnyOf(
                           StartsWith(user->legacy_user_mount_path),
                           StartsWith(user->vault_mount_path))))
    .WillRepeatedly(Return(true));

  mount_args.create_if_missing = true;
  error = MOUNT_ERROR_NONE;
  ASSERT_TRUE(mount->MountCryptohome(up, mount_args, &error));
  ASSERT_EQ(MOUNT_ERROR_NONE, error);
}

TEST_F(MountTest, UserActivityTimestampUpdated) {
  // checks that user activity timestamp is updated during Mount() and
  // periodically while mounted, other Keyset fields remains the same
  scoped_refptr<Mount> mount = new Mount();
  NiceMock<MockTpm> tpm;
  mount->crypto()->set_tpm(&tpm);
  mount->set_shadow_root(kImageDir);
  mount->set_skel_source(kSkelDir);
  mount->set_use_tpm(false);
  set_policy(mount.get(), false, "", false);

  NiceMock<MockPlatform> platform;
  mount->set_platform(&platform);
  mount->crypto()->set_platform(&platform);

  NiceMock<MockHomeDirs> mock_homedirs;
  mount->set_homedirs(&mock_homedirs);
  EXPECT_CALL(mock_homedirs, Init())
    .WillOnce(Return(true));

  EXPECT_CALL(platform, DirectoryExists(kImageDir))
    .WillRepeatedly(Return(true));
  EXPECT_TRUE(DoMountInit(mount.get()));

  InsertTestUsers(&kDefaultUsers[9], 1);
  TestUser* user = &helper_.users[0];
  UsernamePasskey up(user->username, user->passkey);

  EXPECT_CALL(platform,
              CreateDirectory(AnyOf(mount->GetNewUserPath(user->username),
                                    StartsWith(kImageDir))))
    .WillRepeatedly(Return(true));

  user->InjectKeyset(&platform, false);
  user->InjectUserPaths(&platform, chronos_uid_, chronos_gid_,
                        shared_gid_, kDaemonGid);

  std::vector<int> key_indices;
  key_indices.push_back(0);
  EXPECT_CALL(mock_homedirs, GetVaultKeysets(user->obfuscated_username, _))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(key_indices),
                          Return(true)));

  // Mount()
  MountError error;
  EXPECT_CALL(platform, Mount(_, _, _, _))
      .WillOnce(Return(true));
  EXPECT_CALL(platform, Bind(_, _))
      .Times(4)
      .WillRepeatedly(Return(true));
  ASSERT_TRUE(mount->MountCryptohome(up, Mount::MountArgs(), &error));

  // Update the timestamp. Normally it is called in MountTaskMount::Run() in
  // background but here in the test we must call it manually.
  static const int kMagicTimestamp = 123;
  chromeos::Blob updated_keyset;
  EXPECT_CALL(platform, WriteFile(user->keyset_path, _))
    .WillRepeatedly(DoAll(SaveArg<1>(&updated_keyset), Return(true)));
  EXPECT_CALL(platform, GetCurrentTime())
      .WillOnce(Return(base::Time::FromInternalValue(kMagicTimestamp)));
  mount->UpdateCurrentUserActivityTimestamp(0);
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
      .Times(5)
      .WillRepeatedly(Return(true));
  mount->UnmountCryptohome();
  SerializedVaultKeyset serialized2;
  ASSERT_TRUE(serialized2.ParseFromArray(
      static_cast<const unsigned char*>(&updated_keyset[0]),
      updated_keyset.size()));
  ASSERT_TRUE(serialized2.has_last_activity_timestamp());
  EXPECT_EQ(kMagicTimestamp2, serialized2.last_activity_timestamp());

  // Update timestamp again, after user is unmounted. User's activity
  // timestamp must not change this.
  mount->UpdateCurrentUserActivityTimestamp(0);
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
  scoped_refptr<Mount> mount = new Mount();
  NiceMock<MockTpm> tpm;
  NiceMock<MockPlatform> platform;
  mount->set_platform(&platform);

  NiceMock<MockHomeDirs> mock_homedirs;
  mount->set_homedirs(&mock_homedirs);
  EXPECT_CALL(mock_homedirs, Init())
    .WillOnce(Return(true));

  mount->crypto()->set_tpm(&tpm);
  mount->crypto()->set_platform(&platform);
  mount->set_shadow_root(kImageDir);
  mount->set_skel_source(kSkelDir);
  mount->set_use_tpm(false);

  EXPECT_CALL(platform, DirectoryExists(kImageDir))
    .WillRepeatedly(Return(true));
  EXPECT_TRUE(DoMountInit(mount.get()));
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

    EXPECT_TRUE(mount->MountForUser(&session, src, dest0, "", ""));
    EXPECT_TRUE(mount->BindForUser(&session, src, dest1));
    EXPECT_TRUE(mount->MountForUser(&session, src, dest2, "", ""));
    mount->UnmountAllForUser(&session);
    EXPECT_FALSE(mount->UnmountForUser(&session));
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
  AltImageTest() : homedirs_mocked_(true) { }

  void SetUpAltImage(const TestUserInfo *users, int user_count) {
    // Set up fresh users.
    MountTest::SetUp();
    InsertTestUsers(users, user_count);

    // Initialize Mount object.
    mount_ = new Mount();
    mount_->set_platform(&platform_);
    mount_->crypto()->set_tpm(&tpm_);
    mount_->crypto()->set_platform(&platform_);

    mount_->homedirs()->set_platform(&platform_);
    if (homedirs_mocked_) {
      mount_->set_homedirs(&homedirs_);
      EXPECT_CALL(homedirs_, Init())
        .WillOnce(Return(true));
    }

    mount_->set_shadow_root(kImageDir);
    mount_->set_use_tpm(false);
    set_policy(mount_.get(), false, "", false);

    EXPECT_CALL(platform_, DirectoryExists(kImageDir))
      .WillRepeatedly(Return(true));
    EXPECT_TRUE(DoMountInit(mount_.get()));
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
        helper_.users[user].InjectKeyset(&platform_, !homedirs_mocked_);
        if (homedirs_mocked_) {
          key_indices_.push_back(0);
          EXPECT_CALL(homedirs_,
                      GetVaultKeysets(helper_.users[user].obfuscated_username,
                                      _))
              .WillRepeatedly(DoAll(SetArgumentPointee<1>(key_indices_),
                             Return(true)));
        }
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
  scoped_refptr<Mount> mount_;
  NiceMock<MockTpm> tpm_;
  MockPlatform platform_;
  MockHomeDirs homedirs_;
  std::vector<int> key_indices_;
  bool homedirs_mocked_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AltImageTest);
};

class EphemeralNoUserSystemTest : public AltImageTest {
 public:
  EphemeralNoUserSystemTest() { }

  void SetUp() {
    SetUpAltImage(kNoUsers, kNoUserCount);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(EphemeralNoUserSystemTest);
};

TEST_F(EphemeralNoUserSystemTest, OwnerUnknownMountCreateTest) {
  // Checks that when a device is not enterprise enrolled and does not have a
  // known owner, a regular vault is created and mounted.
  set_policy(mount_.get(), false, "", true);

  // Setup a NiceMock to not need to mock the entire creation flow.
  NiceMock<MockPlatform> platform;
  mount_->set_platform(&platform);
  mount_->crypto()->set_platform(&platform);

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
  std::vector<int> key_indices;
  key_indices.push_back(0);
  EXPECT_CALL(homedirs_, GetVaultKeysets(user->obfuscated_username, _))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(key_indices),
                          Return(true)));
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
  ASSERT_TRUE(mount_->MountCryptohome(up,
                                     mount_args,
                                     &error));

  ASSERT_TRUE(mount_->UnmountCryptohome());
}

// TODO(wad) Duplicate these tests with multiple mounts instead of one.

TEST_F(EphemeralNoUserSystemTest, EnterpriseMountNoCreateTest) {
  // Checks that when a device is enterprise enrolled, a tmpfs cryptohome is
  // mounted and no regular vault is created.
  set_policy(mount_.get(), false, "", true);
  mount_->set_enterprise_owned(true);
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

  EXPECT_CALL(platform_, IsDirectoryMounted("test_image_dir/skeleton"))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, Mount(_, _, kEphemeralMountType, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, Unmount("test_image_dir/skeleton", _, _))
      .WillOnce(Return(true));  // Scope mount
  EXPECT_CALL(platform_, IsDirectoryMounted("/home/chronos/user"))
    .WillOnce(Return(false));  // first mount
  EXPECT_CALL(platform_, Bind(_, _))
      .WillRepeatedly(Return(true));

  Mount::MountArgs mount_args;
  mount_args.create_if_missing = true;
  MountError error;
  UsernamePasskey up(user->username, user->passkey);
  ASSERT_TRUE(mount_->MountCryptohome(up, mount_args, &error));
}

TEST_F(EphemeralNoUserSystemTest, OwnerUnknownMountEnsureEphemeralTest) {
  // Checks that when a device is not enterprise enrolled and does not have a
  // known owner, a mount request with the |ensure_ephemeral| flag set fails.
  set_policy(mount_.get(), false, "", false);
  TestUser *user = &helper_.users[0];

  EXPECT_CALL(platform_, Mount(_, _, _, _)).Times(0);

  Mount::MountArgs mount_args;
  mount_args.create_if_missing = true;
  mount_args.ensure_ephemeral = true;
  MountError error;
  UsernamePasskey up(user->username, user->passkey);
  ASSERT_FALSE(mount_->MountCryptohome(up, mount_args, &error));
  ASSERT_EQ(MOUNT_ERROR_FATAL, error);
}

TEST_F(EphemeralNoUserSystemTest, EnterpriseMountEnsureEphemeralTest) {
  // Checks that when a device is enterprise enrolled, a mount request with the
  // |ensure_ephemeral| flag set causes a tmpfs cryptohome to be mounted and no
  // regular vault to be created.
  set_policy(mount_.get(), true, "", false);
  mount_->set_enterprise_owned(true);
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

  EXPECT_CALL(platform_, IsDirectoryMounted("test_image_dir/skeleton"))
    .WillOnce(Return(true))
    .WillOnce(Return(true));
  EXPECT_CALL(platform_, Mount(_, _, kEphemeralMountType, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, Unmount("test_image_dir/skeleton", _, _))
      .WillOnce(Return(true));  // Scope mount
  EXPECT_CALL(platform_, IsDirectoryMounted("/home/chronos/user"))
    .WillOnce(Return(false));  // first mount
  EXPECT_CALL(platform_, Bind(_, _))
      .WillRepeatedly(Return(true));

  Mount::MountArgs mount_args;
  mount_args.create_if_missing = true;
  mount_args.ensure_ephemeral = true;
  MountError error;
  UsernamePasskey up(user->username, user->passkey);
  ASSERT_TRUE(mount_->MountCryptohome(up, mount_args, &error));

  EXPECT_CALL(platform_, Unmount(StartsWith("/home/chronos/u-"), _, _))
      .WillOnce(Return(true));  // user mount
  EXPECT_CALL(platform_, Unmount(StartsWith("/home/user/"), _, _))
      .WillOnce(Return(true));  // user mount
  EXPECT_CALL(platform_, Unmount(StartsWith("/home/root/"), _, _))
      .WillOnce(Return(true));  // user mount
  EXPECT_CALL(platform_, Unmount("/home/chronos/user", _, _))
      .WillOnce(Return(true));  // legacy mount
  EXPECT_CALL(platform_, ClearUserKeyring())
      .WillRepeatedly(Return(0));
  EXPECT_TRUE(mount_->UnmountCryptohome());
}

class EphemeralOwnerOnlySystemTest : public AltImageTest {
 public:
  EphemeralOwnerOnlySystemTest() { }

  void SetUp() {
    SetUpAltImage(kOwnerOnlyUsers, kOwnerOnlyUserCount);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(EphemeralOwnerOnlySystemTest);
};

TEST_F(EphemeralOwnerOnlySystemTest, MountNoCreateTest) {
  // Checks that when a device is not enterprise enrolled and has a known owner,
  // a tmpfs cryptohome is mounted and no regular vault is created.
  TestUser* owner = &helper_.users[3];
  TestUser* user = &helper_.users[0];
  set_policy(mount_.get(), true, owner->username, true);
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
  EXPECT_CALL(platform_, IsDirectoryMounted("test_image_dir/skeleton"))
    .WillOnce(Return(true))
    .WillOnce(Return(true));
  EXPECT_CALL(platform_, Mount(_, _, kEphemeralMountType, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, Unmount("test_image_dir/skeleton", _, _))
      .WillOnce(Return(true));  // Scope mount
  EXPECT_CALL(platform_, IsDirectoryMounted("/home/chronos/user"))
    .WillOnce(Return(false));  // first mount
  EXPECT_CALL(platform_, Bind(_, _))
      .WillRepeatedly(Return(true));

  Mount::MountArgs mount_args;
  mount_args.create_if_missing = true;
  MountError error;
  ASSERT_TRUE(mount_->MountCryptohome(up, mount_args, &error));

  EXPECT_CALL(platform_, Unmount(StartsWith("/home/chronos/u-"), _, _))
      .WillOnce(Return(true));  // user mount
  EXPECT_CALL(platform_, Unmount(StartsWith("/home/user/"), _, _))
      .WillOnce(Return(true));  // user mount
  EXPECT_CALL(platform_, Unmount(StartsWith("/home/root/"), _, _))
      .WillOnce(Return(true));  // user mount
  EXPECT_CALL(platform_, Unmount("/home/chronos/user", _, _))
      .WillOnce(Return(true));  // legacy mount
  EXPECT_CALL(platform_, ClearUserKeyring())
      .WillRepeatedly(Return(0));
  ASSERT_TRUE(mount_->UnmountCryptohome());
}

TEST_F(EphemeralOwnerOnlySystemTest, NonOwnerMountEnsureEphemeralTest) {
  // Checks that when a device is not enterprise enrolled and has a known owner,
  // a mount request for a non-owner user with the |ensure_ephemeral| flag set
  // causes a tmpfs cryptohome to be mounted and no regular vault to be created.
  TestUser* owner = &helper_.users[3];
  TestUser* user = &helper_.users[0];
  set_policy(mount_.get(), true, owner->username, false);
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
  EXPECT_CALL(platform_, IsDirectoryMounted("test_image_dir/skeleton"))
    .WillOnce(Return(true))
    .WillOnce(Return(true));
  EXPECT_CALL(platform_, Mount(_, _, kEphemeralMountType, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, Unmount("test_image_dir/skeleton", _, _))
      .WillOnce(Return(true));  // Scope mount
  EXPECT_CALL(platform_, IsDirectoryMounted("/home/chronos/user"))
    .WillOnce(Return(false));  // first mount
  EXPECT_CALL(platform_, Bind(_, _))
      .WillRepeatedly(Return(true));

  Mount::MountArgs mount_args;
  mount_args.create_if_missing = true;
  mount_args.ensure_ephemeral = true;
  MountError error;
  ASSERT_TRUE(mount_->MountCryptohome(up, mount_args, &error));
}

TEST_F(EphemeralOwnerOnlySystemTest, OwnerMountEnsureEphemeralTest) {
  // Checks that when a device is not enterprise enrolled and has a known owner,
  // a mount request for the owner with the |ensure_ephemeral| flag set fails.
  TestUser* owner = &helper_.users[3];
  set_policy(mount_.get(), true, owner->username, false);
  UsernamePasskey up(owner->username, owner->passkey);

  EXPECT_CALL(platform_, Mount(_, _, _, _)).Times(0);

  Mount::MountArgs mount_args;
  mount_args.create_if_missing = true;
  mount_args.ensure_ephemeral = true;
  MountError error;
  ASSERT_FALSE(mount_->MountCryptohome(up, mount_args, &error));
  ASSERT_EQ(MOUNT_ERROR_FATAL, error);
}

class EphemeralExistingUserSystemTest : public AltImageTest {
 public:
  EphemeralExistingUserSystemTest() { }

  void SetUp() {
    SetUpAltImage(kAlternateUsers, kAlternateUserCount);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(EphemeralExistingUserSystemTest);
};

TEST_F(EphemeralExistingUserSystemTest, OwnerUnknownMountNoRemoveTest) {
  // Checks that when a device is not enterprise enrolled and does not have a
  // known owner, no stale cryptohomes are removed while mounting.
  set_policy(mount_.get(), false, "", true);
  TestUser* user = &helper_.users[0];

  // No c-homes will be removed.  The rest of the mocking just gets us to
  // Mount().
  std::vector<TestUser>::iterator it;
  for (it = helper_.users.begin(); it != helper_.users.end(); ++it)
    it->InjectUserPaths(&platform_, chronos_uid_, chronos_gid_,
                        shared_gid_, kDaemonGid);

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

  std::vector<int> key_indices;
  key_indices.push_back(0);
  EXPECT_CALL(homedirs_, GetVaultKeysets(user->obfuscated_username, _))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(key_indices),
                          Return(true)));

  EXPECT_CALL(platform_, Mount(_, _, _, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, Mount(_, _, kEphemeralMountType, _))
      .Times(0);
  EXPECT_CALL(platform_, Bind(_, _))
      .WillRepeatedly(Return(true));

  Mount::MountArgs mount_args;
  mount_args.create_if_missing = true;
  MountError error;
  user->InjectKeyset(&platform_, false);
  UsernamePasskey up(user->username, user->passkey);
  ASSERT_TRUE(mount_->MountCryptohome(up, mount_args, &error));


  EXPECT_CALL(platform_, Unmount(EndsWith("/mount"), _, _))
      .WillOnce(Return(true));  // user mount
  EXPECT_CALL(platform_, Unmount(StartsWith("/home/chronos/u-"), _, _))
      .WillOnce(Return(true));  // user mount
  EXPECT_CALL(platform_, Unmount(StartsWith("/home/user/"), _, _))
      .WillOnce(Return(true));  // user mount
  EXPECT_CALL(platform_, Unmount(StartsWith("/home/root/"), _, _))
      .WillOnce(Return(true));  // user mount
  EXPECT_CALL(platform_, Unmount("/home/chronos/user", _, _))
      .WillOnce(Return(true));  // legacy mount
  EXPECT_CALL(platform_, ClearUserKeyring())
      .WillRepeatedly(Return(0));
  ASSERT_TRUE(mount_->UnmountCryptohome());
}

TEST_F(EphemeralExistingUserSystemTest, EnterpriseMountRemoveTest) {
  // Checks that when a device is enterprise enrolled, all stale cryptohomes are
  // removed while mounting.
  set_policy(mount_.get(), false, "", true);
  mount_->set_enterprise_owned(true);
  TestUser* user = &helper_.users[0];
  UsernamePasskey up(user->username, user->passkey);

  std::vector<int> expect_deletion;
  expect_deletion.push_back(0);
  expect_deletion.push_back(1);
  expect_deletion.push_back(2);
  expect_deletion.push_back(3);
  PrepareHomedirs(false, false, true, &expect_deletion, NULL);

  // Let Mount know how many vaults there are.
  std::vector<std::string> no_vaults;
  EXPECT_CALL(platform_, EnumerateDirectoryEntries(kImageDir, false, _))
    .WillOnce(DoAll(SetArgumentPointee<2>(vaults_), Return(true)))
    // Don't re-delete on Unmount.
    .WillRepeatedly(DoAll(SetArgumentPointee<2>(no_vaults), Return(true)));
  // Don't say any cryptohomes are mounted
  EXPECT_CALL(platform_, IsDirectoryMountedWith(_, _))
    .WillRepeatedly(Return(false));
  std::vector<std::string> empty;
  EXPECT_CALL(platform_,
      EnumerateDirectoryEntries(AnyOf("/home/root/",
                                      "/home/user/"), _, _))
    .WillRepeatedly(DoAll(SetArgumentPointee<2>(empty), Return(true)));
  EXPECT_CALL(platform_,
      Stat(AnyOf("/home/chronos",
                 mount_->GetNewUserPath(user->username)),
           _))
      .WillRepeatedly(Return(false));
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
  user->InjectUserPaths(&platform_, chronos_uid_, chronos_gid_,
                        shared_gid_, kDaemonGid);
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
  EXPECT_CALL(platform_, IsDirectoryMounted("test_image_dir/skeleton"))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, Mount(_, _, kEphemeralMountType, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, Unmount("test_image_dir/skeleton", _, _))
      .WillOnce(Return(true));  // Scope mount
  EXPECT_CALL(platform_, IsDirectoryMounted("/home/chronos/user"))
    .WillOnce(Return(false));  // first mount
  EXPECT_CALL(platform_, Bind(_, _))
      .WillRepeatedly(Return(true));

  Mount::MountArgs mount_args;
  mount_args.create_if_missing = true;
  MountError error;
  ASSERT_TRUE(mount_->MountCryptohome(up, mount_args, &error));

  EXPECT_CALL(platform_, Unmount(StartsWith("/home/chronos/u-"), _, _))
      .WillOnce(Return(true));  // user mount
  EXPECT_CALL(platform_, Unmount(StartsWith("/home/user/"), _, _))
      .WillOnce(Return(true));  // user mount
  EXPECT_CALL(platform_, Unmount(StartsWith("/home/root/"), _, _))
      .WillOnce(Return(true));  // user mount
  EXPECT_CALL(platform_, Unmount("/home/chronos/user", _, _))
      .WillOnce(Return(true));  // legacy mount
  EXPECT_CALL(platform_, ClearUserKeyring())
      .WillRepeatedly(Return(0));
  ASSERT_TRUE(mount_->UnmountCryptohome());
}

TEST_F(EphemeralExistingUserSystemTest, MountRemoveTest) {
  // Checks that when a device is not enterprise enrolled and has a known owner,
  // all non-owner cryptohomes are removed while mounting.
  TestUser* owner = &helper_.users[3];
  set_policy(mount_.get(), true, owner->username, true);
  TestUser* user = &helper_.users[0];
  UsernamePasskey up(user->username, user->passkey);

  std::vector<int> expect_deletion;
  expect_deletion.push_back(0); // the mounting user shouldn't use be persistent
  expect_deletion.push_back(1);
  expect_deletion.push_back(2);
  // Expect all users but the owner to be removed.
  PrepareHomedirs(false, false, true, &expect_deletion, NULL);

  // Let Mount know how many vaults there are.
  std::vector<std::string> no_vaults;
  EXPECT_CALL(platform_, EnumerateDirectoryEntries(kImageDir, false, _))
    .WillOnce(DoAll(SetArgumentPointee<2>(vaults_), Return(true)))
    // Don't re-delete on Unmount.
    .WillRepeatedly(DoAll(SetArgumentPointee<2>(no_vaults), Return(true)));
  // Don't say any cryptohomes are mounted
  EXPECT_CALL(platform_, IsDirectoryMountedWith(_, _))
    .WillRepeatedly(Return(false));
  std::vector<std::string> empty;
  EXPECT_CALL(platform_,
      EnumerateDirectoryEntries(AnyOf("/home/root/",
                                      "/home/user/"), _, _))
    .WillRepeatedly(DoAll(SetArgumentPointee<2>(empty), Return(true)));
  EXPECT_CALL(platform_,
      Stat(AnyOf("/home/chronos",
                 mount_->GetNewUserPath(user->username)),
           _))
      .WillRepeatedly(Return(false));
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
  user->InjectUserPaths(&platform_, chronos_uid_, chronos_gid_,
                        shared_gid_, kDaemonGid);
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
  EXPECT_CALL(platform_, IsDirectoryMounted("test_image_dir/skeleton"))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, Mount(_, _, kEphemeralMountType, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, Unmount("test_image_dir/skeleton", _, _))
      .WillOnce(Return(true));  // Scope mount
  EXPECT_CALL(platform_, IsDirectoryMounted("/home/chronos/user"))
    .WillOnce(Return(false));  // first mount
  EXPECT_CALL(platform_, Bind(_, _))
      .WillRepeatedly(Return(true));

  Mount::MountArgs mount_args;
  mount_args.create_if_missing = true;
  MountError error;
  ASSERT_TRUE(mount_->MountCryptohome(up, mount_args, &error));

  EXPECT_CALL(platform_, Unmount(StartsWith("/home/chronos/u-"), _, _))
      .WillOnce(Return(true));  // user mount
  EXPECT_CALL(platform_, Unmount(StartsWith("/home/user/"), _, _))
      .WillOnce(Return(true));  // user mount
  EXPECT_CALL(platform_, Unmount(StartsWith("/home/root/"), _, _))
      .WillOnce(Return(true));  // user mount
  EXPECT_CALL(platform_, Unmount("/home/chronos/user", _, _))
      .WillOnce(Return(true));  // legacy mount
  EXPECT_CALL(platform_, ClearUserKeyring())
      .WillRepeatedly(Return(0));
  ASSERT_TRUE(mount_->UnmountCryptohome());
}

TEST_F(EphemeralExistingUserSystemTest, OwnerUnknownUnmountNoRemoveTest) {
  // Checks that when a device is not enterprise enrolled and does not have a
  // known owner, no stale cryptohomes are removed while unmounting.
  set_policy(mount_.get(), false, "", true);
  EXPECT_CALL(platform_, ClearUserKeyring())
    .WillOnce(Return(0));
  ASSERT_TRUE(mount_->UnmountCryptohome());
}

TEST_F(EphemeralExistingUserSystemTest, EnterpriseUnmountRemoveTest) {
  // Checks that when a device is enterprise enrolled, all stale cryptohomes are
  // removed while unmounting.
  set_policy(mount_.get(), false, "", true);
  mount_->set_enterprise_owned(true);

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

  ASSERT_TRUE(mount_->UnmountCryptohome());
}

TEST_F(EphemeralExistingUserSystemTest, UnmountRemoveTest) {
  // Checks that when a device is not enterprise enrolled and has a known owner,
  // all stale cryptohomes are removed while unmounting.
  TestUser* owner = &helper_.users[3];
  set_policy(mount_.get(), true, owner->username, true);
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

  ASSERT_TRUE(mount_->UnmountCryptohome());
}

TEST_F(EphemeralExistingUserSystemTest, NonOwnerMountEnsureEphemeralTest) {
  // Checks that when a device is not enterprise enrolled and has a known owner,
  // a mount request for a non-owner user with the |ensure_ephemeral| flag set
  // causes a tmpfs cryptohome to be mounted, even if a regular vault exists for
  // the user.
  // Since ephemeral users aren't enabled, no vaults will be deleted.
  TestUser* owner = &helper_.users[3];
  set_policy(mount_.get(), true, owner->username, false);
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
      Stat(AnyOf("/home/chronos",
                 mount_->GetNewUserPath(user->username)
                ),
           _))
     .WillRepeatedly(Return(false));
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

  EXPECT_CALL(platform_, IsDirectoryMounted("test_image_dir/skeleton"))
    .WillRepeatedly(Return(true));

  helper_.InjectEphemeralSkeleton(&platform_, kImageDir, false);

  EXPECT_CALL(platform_, Mount(_, _, _, _))
      .Times(0);
  EXPECT_CALL(platform_, IsDirectoryMounted("test_image_dir/skeleton"))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, Mount(_, _, kEphemeralMountType, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, Unmount("test_image_dir/skeleton", _, _))
      .WillOnce(Return(true));  // Scope mount
  EXPECT_CALL(platform_, IsDirectoryMounted("/home/chronos/user"))
    .WillOnce(Return(false));  // first mount
  EXPECT_CALL(platform_, Bind(_, _))
      .WillRepeatedly(Return(true));

  Mount::MountArgs mount_args;
  mount_args.create_if_missing = true;
  mount_args.ensure_ephemeral = true;
  MountError error;
  ASSERT_TRUE(mount_->MountCryptohome(up, mount_args, &error));
}

TEST_F(EphemeralExistingUserSystemTest, EnterpriseMountEnsureEphemeralTest) {
  // Checks that when a device is enterprise enrolled, a mount request with the
  // |ensure_ephemeral| flag set causes a tmpfs cryptohome to be mounted, even
  // if a regular vault exists for the user.
  // Since ephemeral users aren't enabled, no vaults will be deleted.
  set_policy(mount_.get(), true, "", false);
  mount_->set_enterprise_owned(true);

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
      Stat(AnyOf("/home/chronos",
                 mount_->GetNewUserPath(user->username)),
           _))
    .WillRepeatedly(Return(false));
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
  EXPECT_CALL(platform_, IsDirectoryMounted("test_image_dir/skeleton"))
    .WillRepeatedly(Return(true));

  helper_.InjectEphemeralSkeleton(&platform_, kImageDir, false);

  EXPECT_CALL(platform_, Mount(_, _, _, _))
      .Times(0);
  EXPECT_CALL(platform_, IsDirectoryMounted("test_image_dir/skeleton"))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, Mount(_, _, kEphemeralMountType, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, Unmount("test_image_dir/skeleton", _, _))
      .WillOnce(Return(true));  // Scope mount
  EXPECT_CALL(platform_, IsDirectoryMounted("/home/chronos/user"))
    .WillOnce(Return(false));  // first mount
  EXPECT_CALL(platform_, Bind(_, _))
      .WillRepeatedly(Return(true));

  Mount::MountArgs mount_args;
  mount_args.create_if_missing = true;
  mount_args.ensure_ephemeral = true;
  MountError error;
  ASSERT_TRUE(mount_->MountCryptohome(up, mount_args, &error));
}

TEST_F(EphemeralNoUserSystemTest, MountGuestUserDir) {
  struct stat fake_root_st;
  fake_root_st.st_uid = 0;
  fake_root_st.st_gid = 0;
  fake_root_st.st_mode = S_IFDIR | S_IRWXU;
  EXPECT_CALL(platform_, Stat("/home", _))
    .Times(3)
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(fake_root_st),
                          Return(true)));
  EXPECT_CALL(platform_, Stat("/home/root", _))
    .WillOnce(DoAll(SetArgumentPointee<1>(fake_root_st),
                    Return(true)));
  EXPECT_CALL(platform_, Stat(StartsWith("/home/root/"), _))
    .WillOnce(Return(false));
  EXPECT_CALL(platform_, Stat("/home/user", _))
    .WillOnce(DoAll(SetArgumentPointee<1>(fake_root_st),
                    Return(true)));
  EXPECT_CALL(platform_, Stat(StartsWith("/home/user/"), _))
    .WillOnce(Return(false));
  struct stat fake_user_st;
  fake_user_st.st_uid = chronos_uid_;
  fake_user_st.st_gid = chronos_gid_;
  fake_user_st.st_mode = S_IFDIR | S_IRWXU;
  EXPECT_CALL(platform_, Stat("/home/chronos", _))
    .WillOnce(DoAll(SetArgumentPointee<1>(fake_user_st),
                    Return(true)));
  EXPECT_CALL(platform_, CreateDirectory(_))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, SetOwnership(_, _, _))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, SetGroupAccessible(_, _, _))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, IsDirectoryMounted(_))
    .WillOnce(Return(true))
    .WillOnce(Return(false))
    .WillOnce(Return(false));
  EXPECT_CALL(platform_, DirectoryExists(_))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, FileExists(_))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, Mount("guestfs", "test_image_dir/skeleton", _, _))
    .WillOnce(Return(true));
  EXPECT_CALL(platform_, Mount("guestfs", StartsWith("/home/root/"), _, _))
    .WillOnce(Return(true));
  EXPECT_CALL(platform_, Bind("test_image_dir/skeleton",
                              StartsWith("/home/user/")))
    .WillOnce(Return(true));
  EXPECT_CALL(platform_, Bind(StartsWith("/home/user/"),
                              "/home/chronos/user"))
    .WillOnce(Return(true));
  EXPECT_CALL(platform_, Bind(StartsWith("/home/user/"),
                              StartsWith("/home/chronos/u-")))
    .WillOnce(Return(true));

  ASSERT_TRUE(mount_->MountGuestCryptohome());
}

}  // namespace cryptohome
