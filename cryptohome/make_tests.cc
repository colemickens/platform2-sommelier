// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Creates credential stores for testing

#include "cryptohome/make_tests.h"

#include <openssl/evp.h>

#include <algorithm>
#include <base/file_util.h>
#include <base/files/file_path.h>
#include <base/logging.h>
#include <base/memory/ref_counted.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <chromeos/cryptohome.h>
#include <chromeos/secure_blob.h>
#include <chromeos/utility.h>
#include <policy/libpolicy.h>
#include <policy/mock_device_policy.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "cryptohome/crypto.h"
#include "cryptohome/mock_crypto.h"
#include "cryptohome/mock_platform.h"
#include "cryptohome/mock_tpm.h"
#include "cryptohome/mount.h"
#include "cryptohome/username_passkey.h"
#include "cryptohome/vault_keyset.h"

using base::StringPrintf;
using chromeos::SecureBlob;
using ::testing::AnyOf;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::NiceMock;
using ::testing::Mock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SetArgumentPointee;
using ::testing::StartsWith;
using ::testing::_;

namespace cryptohome {

// struct TestUserInfo {
//   const char* username;
//   const char* password;
//   bool create;
// };

const TestUserInfo kDefaultUsers[] = {
  {"testuser0@invalid.domain", "zero", true},
  {"testuser1@invalid.domain", "one", true},
  {"testuser2@invalid.domain", "two", true},
  {"testuser3@invalid.domain", "three", true},
  {"testuser4@invalid.domain", "four", true},
  {"testuser5@invalid.domain", "five", false},
  {"testuser6@invalid.domain", "six", true},
  {"testuser7@invalid.domain", "seven", true},
  {"testuser8@invalid.domain", "eight", true},
  {"testuser9@invalid.domain", "nine", true},
  {"testuser10@invalid.domain", "ten", true},
  {"testuser11@invalid.domain", "eleven", true},
  {"testuser12@invalid.domain", "twelve", false},
  {"testuser13@invalid.domain", "thirteen", true},
};
const size_t kDefaultUserCount = arraysize(kDefaultUsers);

MakeTests::MakeTests() { }

void MakeTests::InitTestData(const std::string& image_dir,
                             const TestUserInfo* test_users,
                             size_t test_user_count) {
  CHECK(system_salt.size()) << "Call SetUpSystemSalt() first";
  users.clear();
  users.resize(test_user_count);
  const TestUserInfo* user_info = test_users;
  for (size_t id = 0; id < test_user_count; ++id, ++user_info) {
    TestUser* user = &users[id];
    user->FromInfo(user_info, image_dir.c_str());
    user->GenerateCredentials();
  }
}

void MakeTests::SetUpSystemSalt() {
  std::string* salt = new std::string(CRYPTOHOME_DEFAULT_SALT_LENGTH, 'A');
  system_salt.resize(salt->size());
  memcpy(&system_salt[0], salt->data(), salt->size());
  chromeos::cryptohome::home::SetSystemSalt(salt);
}

void MakeTests::TearDownSystemSalt() {
  std::string* salt = chromeos::cryptohome::home::GetSystemSalt();
  chromeos::cryptohome::home::SetSystemSalt(NULL);
  delete salt;
}

void MakeTests::InjectSystemSalt(MockPlatform* platform,
                                 const std::string& path) {
  CHECK(chromeos::cryptohome::home::GetSystemSalt());
  EXPECT_CALL(*platform, FileExists(path))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(*platform, GetFileSize(path, _))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(system_salt.size()),
                          Return(true)));
  EXPECT_CALL(*platform, ReadFile(path, _))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(system_salt), Return(true)));
}

void MakeTests::InjectEphemeralSkeleton(MockPlatform* platform,
                                        const std::string& root,
                                        bool exists) {
  const std::string skel = StringPrintf("%s/skeleton", root.c_str());
  EXPECT_CALL(*platform, CreateDirectory(StartsWith(skel)))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(*platform, SetOwnership(StartsWith(skel), _, _))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(*platform, DirectoryExists(StartsWith(skel)))
    .WillRepeatedly(Return(exists));
  EXPECT_CALL(*platform, FileExists(StartsWith(skel)))
    .WillRepeatedly(Return(exists));
  if (!exists) {
    EXPECT_CALL(*platform, SetGroupAccessible(StartsWith(skel), _, _))
      .WillRepeatedly(Return(true));
  }
}


void TestUser::FromInfo(const struct TestUserInfo* info,
                        const char* image_dir) {
  username = info->username;
  password = info->password;
  create = info->create;
  use_key_data = false;
  // Stub system salt must already be in place. See MountTest::SetUp().
  // Sanitized usernames and obfuscated ones differ by case. Accomodate both.
  // TODO(ellyjones) fix this discrepancy!
  sanitized_username = chromeos::cryptohome::home::SanitizeUserName(username);
  obfuscated_username = sanitized_username;
  std::transform(obfuscated_username.begin(),
                 obfuscated_username.end(),
                 obfuscated_username.begin(),
                 ::tolower);
  // Both pass this check though.
  DCHECK(chromeos::cryptohome::home::IsSanitizedUserName(
           obfuscated_username));
  shadow_root = image_dir;
  skel_dir = StringPrintf("%s/skel", image_dir);
  base_path = StringPrintf("%s/%s", image_dir, obfuscated_username.c_str());
  image_path = StringPrintf("%s/image", base_path.c_str());
  vault_path = StringPrintf("%s/vault", base_path.c_str());
  vault_mount_path = StringPrintf("%s/mount", base_path.c_str());
  root_vault_path = StringPrintf("%s/root", vault_path.c_str());
  user_vault_path = StringPrintf("%s/user", vault_path.c_str());
  keyset_path = StringPrintf("%s/master.0", base_path.c_str());
  salt_path = StringPrintf("%s/master.0.salt", base_path.c_str());
  user_salt.assign('A', PKCS5_SALT_LEN);
  mount_prefix =
    chromeos::cryptohome::home::GetUserPathPrefix().DirName().value();
  legacy_user_mount_path = "/home/chronos/user";
  user_mount_path = chromeos::cryptohome::home::GetUserPath(username)
    .StripTrailingSeparators().value();
  user_mount_prefix = chromeos::cryptohome::home::GetUserPathPrefix()
    .StripTrailingSeparators().value();
  root_mount_path = chromeos::cryptohome::home::GetRootPath(username)
    .StripTrailingSeparators().value();
  root_mount_prefix = chromeos::cryptohome::home::GetRootPathPrefix()
    .StripTrailingSeparators().value();
}

void TestUser::GenerateCredentials() {
  std::string* system_salt = chromeos::cryptohome::home::GetSystemSalt();
  chromeos::Blob salt;
  salt.resize(system_salt->size());
  memcpy(&salt.at(0), system_salt->c_str(), system_salt->size());
  NiceMock<MockTpm> tpm;
  NiceMock<MockPlatform> platform;
  NiceMock<MockCrypto> crypto;
  crypto.set_platform(&platform);

  scoped_refptr<Mount> mount = new cryptohome::Mount();
  mount->set_shadow_root(shadow_root);
  mount->set_skel_source(skel_dir);
  mount->set_use_tpm(false);
  NiceMock<policy::MockDevicePolicy>* device_policy =
    new NiceMock<policy::MockDevicePolicy>;
  mount->set_policy_provider(new policy::PolicyProvider(device_policy));
  EXPECT_CALL(*device_policy, LoadPolicy())
    .WillRepeatedly(Return(true));
  std::string salt_path = StringPrintf("%s/salt", shadow_root.c_str());
  int64 salt_size = salt.size();
  EXPECT_CALL(platform, FileExists(salt_path))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform, GetFileSize(salt_path, _))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(salt_size), Return(true)));
  EXPECT_CALL(platform, ReadFile(salt_path, _))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(salt), Return(true)));
  EXPECT_CALL(platform, DirectoryExists(shadow_root))
    .WillRepeatedly(Return(true));
  mount->Init(&platform, &crypto);

  cryptohome::Crypto::PasswordToPasskey(password,
                                        salt,
                                        &passkey);
  UsernamePasskey up(username, passkey);
  if (use_key_data) {
    up.set_key_data(key_data);
  }
  bool created;
  // NOTE! This code gives us generated credentials for credentials tests NOT
  // NOTE! golden credentials to test against.  This means we won't see problems
  // NOTE! if the credentials generation and checking code break together.
  // TODO(wad,ellyjones) Add golden credential tests too.

  // "Old" image path
  EXPECT_CALL(platform, FileExists(image_path))
    .WillRepeatedly(Return(false));
  // Use 'stat' failures to trigger default-allow the creation of the paths.
  EXPECT_CALL(platform,
      Stat(AnyOf("/home",
                 "/home/root",
                 chromeos::cryptohome::home::GetRootPath(username).value(),
                 "/home/user",
                 chromeos::cryptohome::home::GetUserPath(username).value()),
           _))
    .WillRepeatedly(Return(false));
  EXPECT_CALL(platform,
      Stat(AnyOf("/home/chronos",
                 mount->GetNewUserPath(username)),
           _))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(platform, DirectoryExists(vault_path))
    .WillOnce(Return(false));
  EXPECT_CALL(platform, CreateDirectory(_))
    .WillRepeatedly(Return(true));
  // Grab the generated credential
  EXPECT_CALL(platform, WriteFile(keyset_path, _))
    .WillOnce(DoAll(SaveArg<1>(&credentials), Return(true)));
  mount->EnsureCryptohome(up, &created);
  DCHECK(created && credentials.size());
}

void TestUser::InjectKeyset(MockPlatform* platform, bool enumerate) {
  // TODO(wad) Update to support multiple keys
  EXPECT_CALL(*platform, FileExists(StartsWith(keyset_path)))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(*platform, ReadFile(keyset_path, _))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(credentials),
                          Return(true)));
  if (enumerate) {
    MockFileEnumerator* files = new MockFileEnumerator();
    EXPECT_CALL(*platform, GetFileEnumerator(base_path, false, _))
      .WillOnce(Return(files));
    {
      InSequence s;
      // Single key.
      EXPECT_CALL(*files, Next())
        .WillOnce(Return(keyset_path));
      EXPECT_CALL(*files, Next())
        .WillOnce(Return(""));
    }
  }
}

void TestUser::InjectUserPaths(MockPlatform* platform,
                               uid_t chronos_uid,
                               gid_t chronos_gid,
                               gid_t chronos_access_gid,
                               gid_t daemon_gid) {
  scoped_refptr<Mount> temp_mount = new Mount();
  EXPECT_CALL(*platform, FileExists(image_path))
    .WillRepeatedly(Return(false));
  struct stat root_dir;
  memset(&root_dir, 0, sizeof(root_dir));
  root_dir.st_mode = S_IFDIR|S_ISVTX;
  EXPECT_CALL(*platform,
      Stat(
        AnyOf(mount_prefix,
              root_mount_prefix,
              user_mount_prefix,
              root_mount_path,
              user_vault_path), _))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(root_dir), Return(true)));
  // Avoid triggering vault migration.  (Is there another test for that?)
  struct stat root_vault_dir;
  memset(&root_vault_dir, 0, sizeof(root_vault_dir));
  root_vault_dir.st_mode = S_IFDIR|S_ISVTX;
  root_vault_dir.st_uid = 0;
  root_vault_dir.st_gid = daemon_gid;
  EXPECT_CALL(*platform,
      Stat(root_vault_path, _))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(root_vault_dir), Return(true)));
  struct stat user_dir;
  memset(&user_dir, 0, sizeof(user_dir));
  user_dir.st_mode = S_IFDIR;
  user_dir.st_uid = chronos_uid;
  user_dir.st_gid = chronos_access_gid;
  EXPECT_CALL(*platform,
      Stat(AnyOf(user_mount_path,
                 temp_mount->GetNewUserPath(username)), _))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(user_dir), Return(true)));
  struct stat chronos_dir;
  memset(&chronos_dir, 0, sizeof(chronos_dir));
  chronos_dir.st_mode = S_IFDIR;
  chronos_dir.st_uid = chronos_uid;
  chronos_dir.st_gid = chronos_gid;
  EXPECT_CALL(*platform,
      Stat("/home/chronos", _))
      .WillRepeatedly(DoAll(SetArgumentPointee<1>(chronos_dir),
                            Return(true)));
  EXPECT_CALL(*platform,
      DirectoryExists(
        AnyOf(shadow_root,
              mount_prefix,
              StartsWith(legacy_user_mount_path),
              StartsWith(vault_mount_path),
              StartsWith(vault_path))))
    .WillRepeatedly(Return(true));
  // TODO(wad) Bounce this out if needed elsewhere.
  std::string user_vault_mount = StringPrintf("%s/user",
                                              vault_mount_path.c_str());
  std::string new_user_path = temp_mount->GetNewUserPath(username);
  EXPECT_CALL(*platform,
      FileExists(AnyOf(StartsWith(legacy_user_mount_path),
                       StartsWith(vault_mount_path),
                       StartsWith(user_mount_path),
                       StartsWith(root_mount_path),
                       StartsWith(new_user_path),
                       StartsWith(keyset_path))))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(*platform, SetGroupAccessible(
                            AnyOf(StartsWith(legacy_user_mount_path),
                                  StartsWith(user_vault_mount)),
                            chronos_access_gid,
                            _))
    .WillRepeatedly(Return(true));
}

}  // namespace cryptohome
