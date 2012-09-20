// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "homedirs.h"

#include <chromeos/cryptohome.h>
#include <chromeos/secure_blob.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <policy/mock_device_policy.h>

#include "make_tests.h"
#include "mock_platform.h"
#include "mock_tpm.h"
#include "username_passkey.h"
#include "user_oldest_activity_timestamp_cache.h"

namespace cryptohome {
using chromeos::SecureBlob;
using std::string;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SetArgumentPointee;
using ::testing::_;

ACTION_P2(SetOwner, owner_known, owner) {
  if (owner_known)
    *arg0 = owner;
  return owner_known;
}

ACTION_P(SetEphemeralUsersEnabled, ephemeral_users_enabled) {
  *arg0 = ephemeral_users_enabled;
  return true;
}

namespace {
const char *kTestRoot = "alt_test_home_dir";
const char *kTestImage = "alt_test_image_dir";

struct homedir {
  const char *name;
  base::Time::Exploded time;
};

const char *kOwner = "<<OWNER>>";
const struct homedir kHomedirs[] = {
  { "d5510a8dda6d743c46dadd979a61ae5603529742", { 2011, 1, 6, 1 } },
  { "8f995cdee8f0711fd32e1cf6246424002c483d47", { 2011, 2, 2, 1 } },
  { "973b9640e86f6073c6b6e2759ff3cf3084515e61", { 2011, 3, 2, 1 } },
  { kOwner, { 2011, 4, 5, 1 } }
};
const base::Time::Exploded jan1st2011_exploded = { 2011, 1, 6, 1 };
const base::Time time_jan1 = base::Time::FromUTCExploded(jan1st2011_exploded);
const base::Time::Exploded feb1st2011_exploded = { 2011, 2, 2, 1 };
const base::Time time_feb1 = base::Time::FromUTCExploded(feb1st2011_exploded);
const base::Time::Exploded mar1st2011_exploded = { 2011, 3, 2, 1 };
const base::Time time_mar1 = base::Time::FromUTCExploded(mar1st2011_exploded);
}  // namespace

class HomeDirsTest : public ::testing::Test {
 public:
  HomeDirsTest() { }
  virtual ~HomeDirsTest() { }

  void SetUp() {
    homedirs_.set_platform(&platform_);
    homedirs_.set_shadow_root(kTestRoot);
    set_policy(true, kOwner, false);
    homedirs_.timestamp_cache()->Initialize();
    homedirs_.Init();
    FilePath fp = FilePath(kTestRoot);
    file_util::CreateDirectory(fp);
    for (unsigned int i = 0; i < arraysize(kHomedirs); i++) {
      const struct homedir *hd = &kHomedirs[i];
      base::Time t = base::Time::FromUTCExploded(hd->time);
      FilePath path;
      std::string owner;
      homedirs_.GetOwner(&owner);
      if (!strcmp(hd->name, kOwner))
        path = fp.Append(owner);
      else
        path = fp.Append(hd->name);

      homedirs_.timestamp_cache()->AddExistingUser(path, t);
      LOG(ERROR) << "push: " << path.value();
      homedir_paths_.push_back(path.value());
    }
  }

  void set_policy(bool owner_known,
                  const string& owner,
                  bool ephemeral_users_enabled) {
    policy::MockDevicePolicy* device_policy = new policy::MockDevicePolicy();
    EXPECT_CALL(*device_policy, LoadPolicy())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*device_policy, GetOwner(_))
        .WillRepeatedly(SetOwner(owner_known, owner));
    EXPECT_CALL(*device_policy, GetEphemeralUsersEnabled(_))
        .WillRepeatedly(SetEphemeralUsersEnabled(ephemeral_users_enabled));
    homedirs_.own_policy_provider(new policy::PolicyProvider(device_policy));
  }

 protected:
  HomeDirs homedirs_;
  NiceMock<MockPlatform> platform_;
  std::vector<std::string> homedir_paths_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HomeDirsTest);
};

TEST_F(HomeDirsTest, RemoveNonOwnerCryptohomes) {
  // Ensure that RemoveNonOwnerCryptohomes does.
  EXPECT_CALL(platform_, EnumerateDirectoryEntries(kTestRoot, false, _))
    .WillOnce(
        DoAll(SetArgumentPointee<2>(homedir_paths_),
              Return(true)));
  FilePath user_prefix = chromeos::cryptohome::home::GetUserPathPrefix();
  FilePath root_prefix = chromeos::cryptohome::home::GetRootPathPrefix();
  EXPECT_CALL(platform_, EnumerateDirectoryEntries(user_prefix.value(), _, _))
    .WillOnce(Return(true));
  EXPECT_CALL(platform_, EnumerateDirectoryEntries(root_prefix.value(), _, _))
    .WillOnce(Return(true));
  EXPECT_CALL(platform_, DirectoryExists(_))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, IsDirectoryMountedWith(_, _))
    .WillRepeatedly(Return(false));
  EXPECT_CALL(platform_, DeleteFile(homedir_paths_[0], true))
    .WillOnce(Return(true));
  EXPECT_CALL(platform_, DeleteFile(homedir_paths_[1], true))
    .WillOnce(Return(true));
  EXPECT_CALL(platform_, DeleteFile(homedir_paths_[2], true))
    .WillOnce(Return(true));

  homedirs_.RemoveNonOwnerCryptohomes();
}

TEST_F(HomeDirsTest, FreeDiskSpace) {
  // Ensure that the two oldest user directories are deleted, but not any
  // others.
  EXPECT_CALL(platform_, EnumerateDirectoryEntries(kTestRoot, false, _))
    .WillRepeatedly(
        DoAll(SetArgumentPointee<2>(homedir_paths_),
              Return(true)));
  EXPECT_CALL(platform_, AmountOfFreeDiskSpace(kTestRoot))
    .WillOnce(Return(0))
    .WillOnce(Return(0))
    .WillOnce(Return(kMinFreeSpace + 1))
    .WillOnce(Return(kEnoughFreeSpace - 1))
    .WillOnce(Return(kEnoughFreeSpace + 1));
  EXPECT_CALL(platform_, DirectoryExists(_))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, DeleteFile(homedir_paths_[0], true))
    .WillOnce(Return(true));
  EXPECT_CALL(platform_, DeleteFile(homedir_paths_[1], true))
    .WillOnce(Return(true));

  homedirs_.FreeDiskSpace();
}

TEST_F(HomeDirsTest, GoodDecryptTest) {
  // create a HomeDirs instance that points to a good shadow root, test that it
  // properly authenticates against the first key.
  SecureBlob system_salt;
  cryptohome::MakeTests make_tests;
  make_tests.InitTestData(kTestImage, kDefaultUsers, kDefaultUserCount);
  NiceMock<MockTpm> tpm;
  homedirs_.set_shadow_root(kTestImage);   // TODO(ellyjones): wat?
  homedirs_.crypto()->set_tpm(&tpm);
  homedirs_.crypto()->set_use_tpm(false);
  ASSERT_TRUE(homedirs_.GetSystemSalt(&system_salt));
  set_policy(false, "", false);

  cryptohome::SecureBlob passkey;
  cryptohome::Crypto::PasswordToPasskey(kDefaultUsers[1].password,
                                        system_salt, &passkey);
  UsernamePasskey up(kDefaultUsers[1].username, passkey);

  ASSERT_TRUE(homedirs_.AreCredentialsValid(up));
}

TEST_F(HomeDirsTest, BadDecryptTest) {
  // create a HomeDirs instance that points to a good shadow root, test that it
  // properly denies access with a bad passkey
  SecureBlob system_salt;
  cryptohome::MakeTests make_tests;
  make_tests.InitTestData(kTestImage, kDefaultUsers, kDefaultUserCount);
  NiceMock<MockTpm> tpm;
  homedirs_.set_shadow_root(kTestImage);
  homedirs_.crypto()->set_tpm(&tpm);
  homedirs_.crypto()->set_use_tpm(false);
  set_policy(false, "", false);

  cryptohome::SecureBlob passkey;
  cryptohome::Crypto::PasswordToPasskey("bogus", system_salt, &passkey);
  UsernamePasskey up(kDefaultUsers[4].username, passkey);

  ASSERT_FALSE(homedirs_.AreCredentialsValid(up));
}

}  // namespace cryptohome
