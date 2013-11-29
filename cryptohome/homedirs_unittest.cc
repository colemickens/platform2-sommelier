// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "homedirs.h"

#include <base/stringprintf.h>
#include <chromeos/constants/cryptohome.h>
#include <chromeos/cryptohome.h>
#include <chromeos/secure_blob.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <policy/mock_device_policy.h>

#include "make_tests.h"
#include "mock_platform.h"
#include "mock_tpm.h"
#include "mock_user_oldest_activity_timestamp_cache.h"
#include "mock_vault_keyset.h"
#include "mock_vault_keyset_factory.h"
#include "username_passkey.h"

namespace cryptohome {
using chromeos::SecureBlob;
using std::string;
using ::testing::DoAll;
using ::testing::EndsWith;
using ::testing::HasSubstr;
using ::testing::InSequence;
using ::testing::MatchesRegex;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::SaveArg;
using ::testing::SetArgumentPointee;
using ::testing::StartsWith;
using ::testing::StrEq;

using ::testing::_;

extern const char kKeyFile[];
extern const int kKeyFileMax;

ACTION_P2(SetOwner, owner_known, owner) {
  if (owner_known)
    *arg0 = owner;
  return owner_known;
}

ACTION_P(SetEphemeralUsersEnabled, ephemeral_users_enabled) {
  *arg0 = ephemeral_users_enabled;
  return true;
}

ACTION_P(SetCleanUpStrategy, clean_up_strategy) {
  if (!clean_up_strategy.empty()) {
    *arg0 = clean_up_strategy;
    return true;
  }
  return false;
}

namespace {
const char *kTestRoot = "alt_test_home_dir";

struct homedir {
  const char *name;
  base::Time::Exploded time;
};

const char *kOwner = "<<OWNER>>";
// Note, the order is important. These should be oldest to newest.
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
const base::Time::Exploded apr5th2011_exploded = { 2011, 4, 5, 1 };
const base::Time time_apr5 = base::Time::FromUTCExploded(apr5th2011_exploded);

}  // namespace

class HomeDirsTest : public ::testing::Test {
 public:
  HomeDirsTest() { }
  virtual ~HomeDirsTest() { }

  void SetUp() {
    test_helper_.SetUpSystemSalt();
    // TODO(wad) Only generate the user data we need. This is time consuming.
    test_helper_.InitTestData(kTestRoot, kDefaultUsers, kDefaultUserCount);
    homedirs_.set_platform(&platform_);
    homedirs_.crypto()->set_platform(&platform_);
    homedirs_.set_shadow_root(kTestRoot);
    test_helper_.InjectSystemSalt(&platform_,
                                  StringPrintf("%s/salt", kTestRoot));
    set_policy(true, kOwner, false, "");

    // Mount() normally sets this.
    homedirs_.set_old_user_last_activity_time(kOldUserLastActivityTime);
    homedirs_.set_timestamp_cache(&timestamp_cache_);

    homedirs_.Init();
    FilePath fp = FilePath(kTestRoot);
    for (unsigned int i = 0; i < arraysize(kHomedirs); i++) {
      const struct homedir *hd = &kHomedirs[i];
      FilePath path;
      std::string owner;
      homedirs_.GetOwner(&owner);
      path = fp.Append(hd->name);
      if (!strcmp(hd->name, kOwner))
        path = fp.Append(owner);
      homedir_paths_.push_back(path.value());
      base::Time t = base::Time::FromUTCExploded(hd->time);
      homedir_times_.push_back(t);
    }
  }

  void TearDown() {
    test_helper_.TearDownSystemSalt();
  }

  void set_policy(bool owner_known,
                  const string& owner,
                  bool ephemeral_users_enabled,
                  const string& clean_up_strategy) {
    policy::MockDevicePolicy* device_policy = new policy::MockDevicePolicy();
    EXPECT_CALL(*device_policy, LoadPolicy())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*device_policy, GetOwner(_))
        .WillRepeatedly(SetOwner(owner_known, owner));
    EXPECT_CALL(*device_policy, GetEphemeralUsersEnabled(_))
        .WillRepeatedly(SetEphemeralUsersEnabled(ephemeral_users_enabled));
    EXPECT_CALL(*device_policy, GetCleanUpStrategy(_))
        .WillRepeatedly(SetCleanUpStrategy(clean_up_strategy));
    homedirs_.own_policy_provider(new policy::PolicyProvider(device_policy));
  }

 protected:
  HomeDirs homedirs_;
  NiceMock<MockPlatform> platform_;
  std::vector<std::string> homedir_paths_;
  MakeTests test_helper_;
  MockUserOldestActivityTimestampCache timestamp_cache_;
  std::vector<base::Time> homedir_times_;
  MockVaultKeysetFactory vault_keyset_factory_;

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

class FreeDiskSpaceTest : public HomeDirsTest {
 public:
  FreeDiskSpaceTest() { }
  virtual ~FreeDiskSpaceTest() { }
};

TEST_F(FreeDiskSpaceTest, InitializeTimeCacheWithNoTime) {
  // To get to the init logic, we need to fail three kEnoughFreeSpace checks.
  EXPECT_CALL(platform_, AmountOfFreeDiskSpace(kTestRoot))
    .WillOnce(Return(0))
    .WillOnce(Return(0))
    .WillOnce(Return(kEnoughFreeSpace - 1));

  // Expect cache clean ups.
  EXPECT_CALL(platform_, EnumerateDirectoryEntries(kTestRoot, false, _))
    .WillRepeatedly(
        DoAll(SetArgumentPointee<2>(homedir_paths_),
              Return(true)));
  EXPECT_CALL(platform_, DirectoryExists(_))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, IsDirectoryMountedWith(_, _))
    .WillRepeatedly(Return(false));

  // The master.* enumerators (wildcard matcher first)
  EXPECT_CALL(platform_, GetFileEnumerator(_, false, _))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>));
  // Empty enumerators per-user per-cache dirs plus
  // enumerators for empty vaults.
  EXPECT_CALL(platform_, GetFileEnumerator(HasSubstr("user/Cache"), false, _))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>));
  EXPECT_CALL(platform_, GetFileEnumerator(HasSubstr("user/GCache"), false, _))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>));
  // Now cover the actual initialization piece
  EXPECT_CALL(timestamp_cache_, initialized())
    .WillOnce(Return(false));
  EXPECT_CALL(timestamp_cache_, Initialize())
    .Times(1);

  // It then walks the user vault to populate.
  MockVaultKeyset* vk[arraysize(kHomedirs)];
  EXPECT_CALL(vault_keyset_factory_, New(_, _))
    .WillOnce(Return(vk[0] = new MockVaultKeyset()))
    .WillOnce(Return(vk[1] = new MockVaultKeyset()))
    .WillOnce(Return(vk[2] = new MockVaultKeyset()))
    .WillOnce(Return(vk[3] = new MockVaultKeyset()));
  for (size_t i = 0; i < arraysize(vk); ++i) {
    EXPECT_CALL(*vk[i], Load(_))
      .WillRepeatedly(Return(false));
  }
  homedirs_.set_vault_keyset_factory(&vault_keyset_factory_);

  // Expect an addition for all users with no time.
  EXPECT_CALL(timestamp_cache_, AddExistingUserNotime(_))
    .Times(4);

  // Now skip the deletion steps by not having a legit owner.
  set_policy(false, "", false, "");

  EXPECT_TRUE(homedirs_.FreeDiskSpace());
}

TEST_F(FreeDiskSpaceTest, InitializeTimeCacheWithOneTime) {
  // To get to the init logic, we need to fail three kEnoughFreeSpace checks.
  EXPECT_CALL(platform_, AmountOfFreeDiskSpace(kTestRoot))
    .WillOnce(Return(0))
    .WillOnce(Return(0))
    .WillOnce(Return(kEnoughFreeSpace - 1));

  // Expect cache clean ups.
  EXPECT_CALL(platform_, EnumerateDirectoryEntries(kTestRoot, false, _))
    .WillRepeatedly(
        DoAll(SetArgumentPointee<2>(homedir_paths_),
              Return(true)));
  EXPECT_CALL(platform_, DirectoryExists(_))
    .WillRepeatedly(Return(true));
  // The master.* enumerators (wildcard matcher first)
  EXPECT_CALL(platform_, GetFileEnumerator(_, false, _))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>));
  // Empty enumerators per-user per-cache dirs plus
  // enumerators for empty vaults.
  EXPECT_CALL(platform_, GetFileEnumerator(HasSubstr("user/Cache"), false, _))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>));
  EXPECT_CALL(platform_, GetFileEnumerator(HasSubstr("user/GCache"), false, _))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>));

  // Now cover the actual initialization piece
  EXPECT_CALL(timestamp_cache_, initialized())
    .WillOnce(Return(false));
  EXPECT_CALL(timestamp_cache_, Initialize())
    .Times(1);
  // Skip vault keyset loading to cause "Notime".
  EXPECT_CALL(platform_, FileExists(StartsWith(homedir_paths_[0])))
    .WillRepeatedly(Return(true));

  MockVaultKeyset* vk[arraysize(kHomedirs)];
  EXPECT_CALL(vault_keyset_factory_, New(_, _))
    .WillOnce(Return(vk[0] = new MockVaultKeyset()))
    .WillOnce(Return(vk[1] = new MockVaultKeyset()))
    .WillOnce(Return(vk[2] = new MockVaultKeyset()))
    .WillOnce(Return(vk[3] = new MockVaultKeyset()));
  // The first three will have no time.
  size_t i;
  for (i = 0; i < arraysize(vk) - 1; ++i) {
    EXPECT_CALL(*vk[i], Load(_))
      .WillRepeatedly(Return(false));
  }
  // Owner will have a master.0
  NiceMock<MockFileEnumerator> *master0;
  EXPECT_CALL(platform_, GetFileEnumerator(homedir_paths_[3], false, _))
    .WillOnce(Return(master0 = new NiceMock<MockFileEnumerator>));
  EXPECT_CALL(*master0, Next())
    .WillOnce(Return(StringPrintf("%s/%s0",
                       homedir_paths_[3].c_str(), kKeyFile)))
    .WillRepeatedly(Return(""));

  // The owner will have a time.
  EXPECT_CALL(*vk[i], Load(_))
    .WillOnce(Return(true));
  SerializedVaultKeyset serialized;
  serialized.set_last_activity_timestamp(homedir_times_[3].ToInternalValue());
  EXPECT_CALL(*vk[i], serialized())
    .Times(2)
    .WillRepeatedly(ReturnRef(serialized));
  homedirs_.set_vault_keyset_factory(&vault_keyset_factory_);

  // Expect an addition for all users with no time.
  EXPECT_CALL(timestamp_cache_, AddExistingUserNotime(_))
    .Times(3);
  // Adding the owner
  EXPECT_CALL(timestamp_cache_,
              AddExistingUser(FilePath(homedir_paths_[3]), homedir_times_[3]))
    .Times(1);

  // Now skip the deletion steps by not having a legit owner.
  set_policy(false, "", false, "");

  EXPECT_TRUE(homedirs_.FreeDiskSpace());
}

TEST_F(FreeDiskSpaceTest, NoCacheCleanup) {
  // Pretend we have lots of free space
  EXPECT_CALL(platform_, AmountOfFreeDiskSpace(kTestRoot))
    .WillOnce(Return(kMinFreeSpaceInBytes + 1));
  EXPECT_FALSE(homedirs_.FreeDiskSpace());
}

TEST_F(FreeDiskSpaceTest, OnlyCacheCleanup) {
  // Only clean up the Cache data. Not GCache, etc.
  EXPECT_CALL(platform_, EnumerateDirectoryEntries(kTestRoot, false, _))
    .WillRepeatedly(
        DoAll(SetArgumentPointee<2>(homedir_paths_),
              Return(true)));

  EXPECT_CALL(platform_, AmountOfFreeDiskSpace(kTestRoot))
    .WillOnce(Return(0))
    .WillOnce(Return(kEnoughFreeSpace + 1));
  EXPECT_CALL(platform_, DirectoryExists(_))
    .WillRepeatedly(Return(true));
  // Empty enumerators per-user per-cache dirs
  NiceMock<MockFileEnumerator>* fe[arraysize(kHomedirs)];
  EXPECT_CALL(platform_, GetFileEnumerator(_, false, _))
    .WillOnce(Return(fe[0] = new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(fe[1] = new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(fe[2] = new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(fe[3] = new NiceMock<MockFileEnumerator>));
  // Exercise the delete file path.
  for (size_t f = 0; f < arraysize(fe); ++f) {
    EXPECT_CALL(*fe[f], Next())
      .WillOnce(Return(StringPrintf("%s/%s", homedir_paths_[f].c_str(),
                                             "Cache/foo")))
      .WillRepeatedly(Return(""));
  }
  EXPECT_CALL(platform_, DeleteFile(EndsWith("/Cache/foo"), true))
    .Times(4)
    .WillRepeatedly(Return(true));

  EXPECT_TRUE(homedirs_.FreeDiskSpace());
}

TEST_F(FreeDiskSpaceTest, CacheAndGCacheCleanup) {
  EXPECT_CALL(platform_, EnumerateDirectoryEntries(kTestRoot, false, _))
    .WillRepeatedly(
        DoAll(SetArgumentPointee<2>(homedir_paths_),
              Return(true)));

  EXPECT_CALL(platform_, AmountOfFreeDiskSpace(kTestRoot))
    .WillOnce(Return(0))
    .WillOnce(Return(0))
    .WillOnce(Return(kEnoughFreeSpace + 1));
  EXPECT_CALL(platform_, DirectoryExists(_))
    .WillRepeatedly(Return(true));
  // Empty enumerators per-user per-cache dirs
  NiceMock<MockFileEnumerator>* fe[arraysize(kHomedirs)];
  EXPECT_CALL(platform_, GetFileEnumerator(EndsWith("/Cache"), false, _))
    .WillOnce(Return(fe[0] = new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(fe[1] = new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(fe[2] = new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(fe[3] = new NiceMock<MockFileEnumerator>));
  // Exercise the delete file path.
  for (size_t f = 0; f < arraysize(fe); ++f) {
    EXPECT_CALL(*fe[f], Next())
      .WillOnce(Return(StringPrintf("%s/%s", homedir_paths_[f].c_str(),
                                             "Cache/foo")))
      .WillRepeatedly(Return(""));
  }
  EXPECT_CALL(platform_, DeleteFile(EndsWith("/Cache/foo"), true))
    .Times(4)
    .WillRepeatedly(Return(true));
  // Repeat for GCache
  EXPECT_CALL(platform_, GetFileEnumerator(EndsWith("GCache/v1/tmp"), false, _))
    .WillOnce(Return(fe[0] = new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(fe[1] = new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(fe[2] = new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(fe[3] = new NiceMock<MockFileEnumerator>));
  // Exercise the delete file path.
  for (size_t f = 0; f < arraysize(fe); ++f) {
    EXPECT_CALL(*fe[f], Next())
      .WillOnce(Return(StringPrintf("%s/%s", homedir_paths_[f].c_str(),
                                             "GCache/v1/tmp/foo")))
      .WillRepeatedly(Return(""));
  }
  EXPECT_CALL(platform_, DeleteFile(EndsWith("/GCache/v1/tmp/foo"), true))
    .Times(4)
    .WillRepeatedly(Return(true));

  EXPECT_TRUE(homedirs_.FreeDiskSpace());
}

TEST_F(FreeDiskSpaceTest, OldUsersCleanupNoTimestamp) {
  // Removes old (except owner and the current one, if any) even if
  // users had no oldest activity timestamp.
  //
  // This requires at least one user have a valid timestamp that is older than
  // the delta. If not, all the NoTimestamp users will stick around.  I believe
  // the supposition is that if there are no timestamps, the vault keysets are
  // either invalid or predate timestamping.  If _any_ user has a timestamp
  // that is old enough, then all unmarked ones must also be old enough.  We
  // may want to consider adding Stat() support to stop relying on this
  // assumption.

  EXPECT_CALL(platform_, EnumerateDirectoryEntries(kTestRoot, false, _))
    .WillRepeatedly(
        DoAll(SetArgumentPointee<2>(homedir_paths_),
              Return(true)));

  EXPECT_CALL(platform_, AmountOfFreeDiskSpace(kTestRoot))
    .WillOnce(Return(0))
    .WillRepeatedly(Return(kEnoughFreeSpace - 1));
  EXPECT_CALL(platform_, DirectoryExists(_))
    .WillRepeatedly(Return(true));
  // Don't bother with files to delete.
  EXPECT_CALL(platform_, GetFileEnumerator(EndsWith("/Cache"), false, _))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>));
  EXPECT_CALL(platform_, GetFileEnumerator(EndsWith("GCache/v1/tmp"), false, _))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>));
  // Nothing is mounted.
  EXPECT_CALL(platform_, IsDirectoryMountedWith(_, _))
    .WillRepeatedly(Return(false));
  EXPECT_CALL(timestamp_cache_, initialized())
    .WillRepeatedly(Return(true));

  // Now pretend that one user had a time, so that we have a non-0
  // oldest time.
  EXPECT_CALL(timestamp_cache_, oldest_known_timestamp())
    .WillOnce(Return(base::Time::Now() - kOldUserLastActivityTime))
    .WillOnce(Return(base::Time::Now() - kOldUserLastActivityTime))
    .WillOnce(Return(base::Time::Now() - kOldUserLastActivityTime))
    .WillOnce(Return(base::Time::Now() - kOldUserLastActivityTime))
    .WillOnce(Return(base::Time::Now() - kOldUserLastActivityTime))
    .WillOnce(Return(base::Time::Now() - kOldUserLastActivityTime))
    .WillOnce(Return(base::Time::Now() - kOldUserLastActivityTime))
    .WillOnce(Return(base::Time::Now() - kOldUserLastActivityTime))
    .WillOnce(Return(base::Time())); // end with is_null()

  EXPECT_CALL(timestamp_cache_, RemoveOldestUser())
    .WillOnce(Return(FilePath(homedir_paths_[0])))
    .WillOnce(Return(FilePath(homedir_paths_[1])))
    .WillOnce(Return(FilePath(homedir_paths_[2])))
    .WillOnce(Return(FilePath(homedir_paths_[3])));

  EXPECT_CALL(platform_, DeleteFile(_, true))
    .Times(arraysize(kHomedirs)-1)  // All but the owner
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, DeleteFile(homedir_paths_[3], true))
    .Times(0);
  EXPECT_TRUE(homedirs_.FreeDiskSpace());
}

TEST_F(FreeDiskSpaceTest, CleanUpOneOldUser) {
  // Ensure that the oldest user directory deleted, but not any
  // others.
  EXPECT_CALL(timestamp_cache_, initialized())
    .WillRepeatedly(Return(true));

  // Move this loop to a helper just for these expects.
  EXPECT_CALL(timestamp_cache_, RemoveOldestUser())
    .WillOnce(Return(FilePath(homedir_paths_[0])));
  // All timestamps are walked but the last one fails the time
  // test so no null time is needed.
  EXPECT_CALL(timestamp_cache_, oldest_known_timestamp())
    .WillOnce(Return(homedir_times_[0]))
    .WillOnce(Return(homedir_times_[0])); // Loop ends early.

  EXPECT_CALL(platform_, EnumerateDirectoryEntries(kTestRoot, false, _))
    .WillRepeatedly(
        DoAll(SetArgumentPointee<2>(homedir_paths_),
              Return(true)));

  EXPECT_CALL(platform_, AmountOfFreeDiskSpace(kTestRoot))
    .WillOnce(Return(0))
    .WillOnce(Return(0))
    .WillOnce(Return(kMinFreeSpaceInBytes + 1))
    .WillOnce(Return(kEnoughFreeSpace + 1));
  EXPECT_CALL(platform_, DirectoryExists(_))
    .WillRepeatedly(Return(true));
  // Empty enumerators per-user per-cache, per-gcache dirs
  EXPECT_CALL(platform_, GetFileEnumerator(_, false, _))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>));
  EXPECT_CALL(platform_, DeleteFile(homedir_paths_[0], true))
    .WillOnce(Return(true));

  EXPECT_TRUE(homedirs_.FreeDiskSpace());
}

TEST_F(FreeDiskSpaceTest, CleanUpMultipleOldUsers) {
  // Ensure that the two oldest user directories are deleted, but not any
  // others.
  EXPECT_CALL(timestamp_cache_, initialized())
    .WillRepeatedly(Return(true));

  // Move this loop to a helper just for these expects.
  EXPECT_CALL(timestamp_cache_, RemoveOldestUser())
    .WillOnce(Return(FilePath(homedir_paths_[0])))
    .WillOnce(Return(FilePath(homedir_paths_[1])));
  // All timestamps are walked but the last one fails the time
  // test so no null time is needed.
  EXPECT_CALL(timestamp_cache_, oldest_known_timestamp())
    .WillOnce(Return(homedir_times_[0]))
    .WillOnce(Return(homedir_times_[0]))
    .WillOnce(Return(homedir_times_[1]))
    .WillOnce(Return(homedir_times_[1]));

  EXPECT_CALL(platform_, EnumerateDirectoryEntries(kTestRoot, false, _))
    .WillRepeatedly(
        DoAll(SetArgumentPointee<2>(homedir_paths_),
              Return(true)));

  EXPECT_CALL(platform_, AmountOfFreeDiskSpace(kTestRoot))
    .WillOnce(Return(0))
    .WillOnce(Return(0))
    .WillOnce(Return(kMinFreeSpaceInBytes + 1))
    .WillOnce(Return(kEnoughFreeSpace - 1))
    .WillOnce(Return(kEnoughFreeSpace + 1));
  EXPECT_CALL(platform_, DirectoryExists(_))
    .WillRepeatedly(Return(true));
  // Empty enumerators per-user per-cache dirs
  EXPECT_CALL(platform_, GetFileEnumerator(_, false, _))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>));
  EXPECT_CALL(platform_, DeleteFile(homedir_paths_[0], true))
    .WillOnce(Return(true));
  EXPECT_CALL(platform_, DeleteFile(homedir_paths_[1], true))
    .WillOnce(Return(true));

  EXPECT_TRUE(homedirs_.FreeDiskSpace());
}

TEST_F(FreeDiskSpaceTest, EnterpriseCleanUpAllUsers) {
   set_policy(true, "", false, "");
   homedirs_.set_enterprise_owned(true);

  // Ensure that the two oldest user directories are deleted, but not any
  // others.
  EXPECT_CALL(timestamp_cache_, initialized())
    .WillRepeatedly(Return(true));

  // Move this loop to a helper just for these expects.
  EXPECT_CALL(timestamp_cache_, RemoveOldestUser())
    .WillOnce(Return(FilePath(homedir_paths_[1])))
    .WillOnce(Return(FilePath(homedir_paths_[2])))
    .WillOnce(Return(FilePath(homedir_paths_[3])))
    .WillOnce(Return(FilePath(homedir_paths_[0])));
  // Pretend only user 0 has a timestamp. All the others
  // will be deleted first.
  EXPECT_CALL(timestamp_cache_, oldest_known_timestamp())
    .WillOnce(Return(homedir_times_[0]))
    .WillOnce(Return(homedir_times_[0]))
    .WillOnce(Return(homedir_times_[0]))
    .WillOnce(Return(homedir_times_[0]))
    .WillOnce(Return(homedir_times_[0]))
    .WillOnce(Return(homedir_times_[0]))
    .WillOnce(Return(homedir_times_[0]))
    .WillOnce(Return(homedir_times_[0]))
    .WillOnce(Return(base::Time())); // No more users.

  EXPECT_CALL(platform_, EnumerateDirectoryEntries(kTestRoot, false, _))
    .WillRepeatedly(
        DoAll(SetArgumentPointee<2>(homedir_paths_),
              Return(true)));

  EXPECT_CALL(platform_, AmountOfFreeDiskSpace(kTestRoot))
    .WillRepeatedly(Return(0));
  EXPECT_CALL(platform_, DirectoryExists(_))
    .WillRepeatedly(Return(true));
  // Empty enumerators per-user per-cache dirs
  EXPECT_CALL(platform_, GetFileEnumerator(_, false, _))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>));
  EXPECT_CALL(platform_, DeleteFile(homedir_paths_[1], true))
    .WillOnce(Return(true));
  EXPECT_CALL(platform_, DeleteFile(homedir_paths_[2], true))
    .WillOnce(Return(true));
  EXPECT_CALL(platform_, DeleteFile(homedir_paths_[3], true))
    .WillOnce(Return(true));
  EXPECT_CALL(platform_, DeleteFile(homedir_paths_[0], true))
    .WillOnce(Return(true));

  EXPECT_TRUE(homedirs_.FreeDiskSpace());
}

TEST_F(FreeDiskSpaceTest, EnterpriseLRUPolicyCleanUpAllUsers) {
   set_policy(true, "", false, "remove-lru");
   homedirs_.set_enterprise_owned(true);

  // Ensure that the two oldest user directories are deleted, but not any
  // others.
  EXPECT_CALL(timestamp_cache_, initialized())
    .WillRepeatedly(Return(true));

  // Move this loop to a helper just for these expects.
  EXPECT_CALL(timestamp_cache_, RemoveOldestUser())
    .WillOnce(Return(FilePath(homedir_paths_[1])))
    .WillOnce(Return(FilePath(homedir_paths_[2])))
    .WillOnce(Return(FilePath(homedir_paths_[3])))
    .WillOnce(Return(FilePath(homedir_paths_[0])));
  // Always return current time, so they don't be deleted if policy is not
  // applied.
  EXPECT_CALL(timestamp_cache_, oldest_known_timestamp())
    .WillOnce(Return(base::Time::Now()))
    .WillOnce(Return(base::Time::Now()))
    .WillOnce(Return(base::Time::Now()))
    .WillOnce(Return(base::Time::Now()))
    .WillOnce(Return(base::Time::Now()))
    .WillOnce(Return(base::Time::Now()))
    .WillOnce(Return(base::Time::Now()))
    .WillOnce(Return(base::Time::Now()))
    .WillOnce(Return(base::Time())); // No more users.

  EXPECT_CALL(platform_, EnumerateDirectoryEntries(kTestRoot, false, _))
    .WillRepeatedly(
        DoAll(SetArgumentPointee<2>(homedir_paths_),
              Return(true)));

  EXPECT_CALL(platform_, AmountOfFreeDiskSpace(kTestRoot))
    .WillRepeatedly(Return(0));
  EXPECT_CALL(platform_, DirectoryExists(_))
    .WillRepeatedly(Return(true));
  // Empty enumerators per-user per-cache dirs
  EXPECT_CALL(platform_, GetFileEnumerator(_, false, _))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>));
  EXPECT_CALL(platform_, DeleteFile(homedir_paths_[1], true))
    .WillOnce(Return(true));
  EXPECT_CALL(platform_, DeleteFile(homedir_paths_[2], true))
    .WillOnce(Return(true));
  EXPECT_CALL(platform_, DeleteFile(homedir_paths_[3], true))
    .WillOnce(Return(true));
  EXPECT_CALL(platform_, DeleteFile(homedir_paths_[0], true))
    .WillOnce(Return(true));

  EXPECT_TRUE(homedirs_.FreeDiskSpace());
}

TEST_F(FreeDiskSpaceTest, CleanUpMultipleNonadjacentOldUsers) {
  // Ensure that the two oldest user directories are deleted, but not any
  // others.  The owner is inserted in the middle.
  EXPECT_CALL(timestamp_cache_, initialized())
    .WillRepeatedly(Return(true));

  // Move this loop to a helper just for these expects.
  EXPECT_CALL(timestamp_cache_, RemoveOldestUser())
    .WillOnce(Return(FilePath(homedir_paths_[0])))
    .WillOnce(Return(FilePath(homedir_paths_[3])))
    .WillOnce(Return(FilePath(homedir_paths_[1])));
  // All timestamps are walked but the last one fails the time
  // test so no null time is needed.
  EXPECT_CALL(timestamp_cache_, oldest_known_timestamp())
    .WillOnce(Return(homedir_times_[0]))
    .WillOnce(Return(homedir_times_[0]))
    .WillOnce(Return(homedir_times_[1]))  // Treat the owner as having the
    .WillOnce(Return(homedir_times_[1]))  // same time.
    .WillOnce(Return(homedir_times_[1]))
    .WillOnce(Return(homedir_times_[1]))
    .WillOnce(Return(base::Time::Now()))
    .WillOnce(Return(base::Time::Now())); // Stop looping with current time

  EXPECT_CALL(platform_, EnumerateDirectoryEntries(kTestRoot, false, _))
    .WillRepeatedly(
        DoAll(SetArgumentPointee<2>(homedir_paths_),
              Return(true)));

  EXPECT_CALL(platform_, AmountOfFreeDiskSpace(kTestRoot))
    .WillOnce(Return(0))
    .WillOnce(Return(0))
    .WillOnce(Return(kMinFreeSpaceInBytes + 1))
    .WillRepeatedly(Return(kEnoughFreeSpace - 1));
  EXPECT_CALL(platform_, DirectoryExists(_))
    .WillRepeatedly(Return(true));
  // Empty enumerators per-user per-cache dirs
  EXPECT_CALL(platform_, GetFileEnumerator(_, false, _))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>));
  EXPECT_CALL(platform_, DeleteFile(homedir_paths_[0], true))
    .WillOnce(Return(true));
  EXPECT_CALL(platform_, DeleteFile(homedir_paths_[1], true))
    .WillOnce(Return(true));
  // Ensure the owner isn't deleted!
  EXPECT_CALL(platform_, DeleteFile(homedir_paths_[3], true))
    .Times(0);

  EXPECT_TRUE(homedirs_.FreeDiskSpace());
}

TEST_F(FreeDiskSpaceTest, NoOwnerNoEnterpriseNoCleanup) {
  // Ensure that no users are deleted with no owner/enterprise-owner.
  EXPECT_CALL(platform_, AmountOfFreeDiskSpace(kTestRoot))
    .WillRepeatedly(Return(0));

  // Expect cache clean ups.
  EXPECT_CALL(platform_, EnumerateDirectoryEntries(kTestRoot, false, _))
    .WillRepeatedly(
        DoAll(SetArgumentPointee<2>(homedir_paths_),
              Return(true)));
  EXPECT_CALL(platform_, DirectoryExists(_))
    .WillRepeatedly(Return(true));
  // Empty enumerators per-user per-cache dirs.
  // That means no Delete calls for (G)Cache.
  EXPECT_CALL(platform_, GetFileEnumerator(_, false, _))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>));

  // Skip re-init
  EXPECT_CALL(timestamp_cache_, initialized())
    .WillOnce(Return(true));

  // No user deletions!
  EXPECT_CALL(platform_, DeleteFile(homedir_paths_[0], true))
    .Times(0);
  EXPECT_CALL(platform_, DeleteFile(homedir_paths_[1], true))
    .Times(0);
  EXPECT_CALL(platform_, DeleteFile(homedir_paths_[2], true))
    .Times(0);
  EXPECT_CALL(platform_, DeleteFile(homedir_paths_[3], true))
    .Times(0);

  // Now skip the deletion steps by not having a legit owner.
  set_policy(false, "", false, "");

  EXPECT_TRUE(homedirs_.FreeDiskSpace());
}

TEST_F(FreeDiskSpaceTest, ConsumerEphemeralUsers) {
  // When ephemeral users are enabled, no cryptohomes are kept except the owner.
  set_policy(true, kOwner, true, "");
  // homedirs_.set_enterprise_owned(true);

  EXPECT_CALL(platform_, EnumerateDirectoryEntries(kTestRoot, false, _))
    .WillRepeatedly(
        DoAll(SetArgumentPointee<2>(homedir_paths_),
              Return(true)));
  EXPECT_CALL(platform_,
      EnumerateDirectoryEntries(
          chromeos::cryptohome::home::GetUserPathPrefix().value(),
          false, _))
    .WillRepeatedly(
        DoAll(SetArgumentPointee<2>(homedir_paths_),
              Return(true)));
  EXPECT_CALL(platform_,
      EnumerateDirectoryEntries(
          chromeos::cryptohome::home::GetRootPathPrefix().value(),
          false, _))
    .WillRepeatedly(
        DoAll(SetArgumentPointee<2>(homedir_paths_),
              Return(true)));

  EXPECT_CALL(platform_, AmountOfFreeDiskSpace(kTestRoot))
    .WillOnce(Return(kMinFreeSpaceInBytes - 1));

  EXPECT_CALL(platform_, DirectoryExists(_))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, DeleteFile(homedir_paths_[0], true))
    .WillOnce(Return(true))  // vault
    .WillOnce(Return(true))  // user
    .WillOnce(Return(true));  // root
  EXPECT_CALL(platform_, DeleteFile(homedir_paths_[1], true))
    .WillOnce(Return(true))
    .WillOnce(Return(true))
    .WillOnce(Return(true));
  EXPECT_CALL(platform_, DeleteFile(homedir_paths_[2], true))
    .WillOnce(Return(true))
    .WillOnce(Return(true))
    .WillOnce(Return(true));
  // Ensure the owner isn't deleted!
  EXPECT_CALL(platform_, DeleteFile(homedir_paths_[3], true))
    .Times(0);

  EXPECT_TRUE(homedirs_.FreeDiskSpace());
}


TEST_F(FreeDiskSpaceTest, EnterpriseEphemeralUsers) {
  // When ephemeral users are enabled, no cryptohomes are kept except the owner.
  set_policy(true, "", true, "");
  homedirs_.set_enterprise_owned(true);

  EXPECT_CALL(platform_, EnumerateDirectoryEntries(kTestRoot, false, _))
    .WillRepeatedly(
        DoAll(SetArgumentPointee<2>(homedir_paths_),
              Return(true)));
  EXPECT_CALL(platform_,
      EnumerateDirectoryEntries(
          chromeos::cryptohome::home::GetUserPathPrefix().value(),
          false, _))
    .WillRepeatedly(
        DoAll(SetArgumentPointee<2>(homedir_paths_),
              Return(true)));
  EXPECT_CALL(platform_,
      EnumerateDirectoryEntries(
          chromeos::cryptohome::home::GetRootPathPrefix().value(),
          false, _))
    .WillRepeatedly(
        DoAll(SetArgumentPointee<2>(homedir_paths_),
              Return(true)));

  EXPECT_CALL(platform_, AmountOfFreeDiskSpace(kTestRoot))
    .WillOnce(Return(kMinFreeSpaceInBytes - 1));

  EXPECT_CALL(platform_, DirectoryExists(_))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_, DeleteFile(homedir_paths_[0], true))
    .WillOnce(Return(true))  // vault
    .WillOnce(Return(true))  // user
    .WillOnce(Return(true));  // root
  EXPECT_CALL(platform_, DeleteFile(homedir_paths_[1], true))
    .WillOnce(Return(true))
    .WillOnce(Return(true))
    .WillOnce(Return(true));
  EXPECT_CALL(platform_, DeleteFile(homedir_paths_[2], true))
    .WillOnce(Return(true))
    .WillOnce(Return(true))
    .WillOnce(Return(true));
  EXPECT_CALL(platform_, DeleteFile(homedir_paths_[3], true))
    .WillOnce(Return(true))
    .WillOnce(Return(true))
    .WillOnce(Return(true));

  EXPECT_TRUE(homedirs_.FreeDiskSpace());
}

TEST_F(FreeDiskSpaceTest, DontCleanUpOneOldMountedUser) {
  // Ensure that the oldest user isn't deleted because
  // it appears to be mounted.
  EXPECT_CALL(timestamp_cache_, initialized())
    .WillRepeatedly(Return(true));

  // This will only be called once (see time below).
  EXPECT_CALL(timestamp_cache_, RemoveOldestUser())
    .WillOnce(Return(FilePath(homedir_paths_[0])));
  // Only mark user 0 as old.
  EXPECT_CALL(timestamp_cache_, oldest_known_timestamp())
    .WillOnce(Return(homedir_times_[0]))
    .WillOnce(Return(homedir_times_[0]))
    .WillOnce(Return(base::Time::Now()))
    .WillOnce(Return(base::Time::Now()));

  EXPECT_CALL(platform_, EnumerateDirectoryEntries(kTestRoot, false, _))
    .WillRepeatedly(
        DoAll(SetArgumentPointee<2>(homedir_paths_),
              Return(true)));

  EXPECT_CALL(platform_, AmountOfFreeDiskSpace(kTestRoot))
    .WillRepeatedly(Return(0));
  EXPECT_CALL(platform_, DirectoryExists(_))
    .WillRepeatedly(Return(true));

  // Ensure the mounted user never has (G)Cache traversed!
  EXPECT_CALL(platform_, GetFileEnumerator(
      StartsWith(homedir_paths_[0]), false, _))
    .Times(0);

  // Empty enumerators per-user per-cache dirs
  EXPECT_CALL(platform_, GetFileEnumerator(_, false, _))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>))
    .WillOnce(Return(new NiceMock<MockFileEnumerator>));

  EXPECT_CALL(platform_,
      IsDirectoryMountedWith(StartsWith(homedir_paths_[0]), _))
    .Times(3)  // Cache, GCache, user removal
    .WillRepeatedly(Return(true));
  for (size_t i = 1; i < arraysize(kHomedirs); ++i) {
    EXPECT_CALL(platform_,
        IsDirectoryMountedWith(StartsWith(homedir_paths_[i]), _))
      .Times(2) // Cache, GCache
      .WillRepeatedly(Return(false));
  }

  EXPECT_TRUE(homedirs_.FreeDiskSpace());
}


TEST_F(HomeDirsTest, GoodDecryptTest) {
  // create a HomeDirs instance that points to a good shadow root, test that it
  // properly authenticates against the first key.
  SecureBlob system_salt;
  NiceMock<MockTpm> tpm;
  homedirs_.crypto()->set_tpm(&tpm);
  homedirs_.crypto()->set_use_tpm(false);
  ASSERT_TRUE(homedirs_.GetSystemSalt(&system_salt));
  set_policy(false, "", false, "");

  test_helper_.users[1].InjectKeyset(&platform_);
  cryptohome::SecureBlob passkey;
  cryptohome::Crypto::PasswordToPasskey(test_helper_.users[1].password,
                                        system_salt, &passkey);
  UsernamePasskey up(test_helper_.users[1].username, passkey);

  ASSERT_TRUE(homedirs_.AreCredentialsValid(up));
}

TEST_F(HomeDirsTest, BadDecryptTest) {
  // create a HomeDirs instance that points to a good shadow root, test that it
  // properly denies access with a bad passkey
  SecureBlob system_salt;
  NiceMock<MockTpm> tpm;
  homedirs_.crypto()->set_tpm(&tpm);
  homedirs_.crypto()->set_use_tpm(false);
  set_policy(false, "", false, "");

  test_helper_.users[4].InjectKeyset(&platform_);

  cryptohome::SecureBlob passkey;
  cryptohome::Crypto::PasswordToPasskey("bogus", system_salt, &passkey);
  UsernamePasskey up(test_helper_.users[4].username, passkey);

  ASSERT_FALSE(homedirs_.AreCredentialsValid(up));
}

class KeysetManagementTest : public HomeDirsTest {
 public:
  KeysetManagementTest() { }
  virtual ~KeysetManagementTest() { }

  virtual bool VkDecrypt0(const chromeos::SecureBlob& key) {
    return memcmp(key.const_data(), keys_[0].const_data(),
                  key.size()) == 0;
  }

  virtual void KeysetSetUp() {
    NiceMock<MockTpm> tpm;
    homedirs_.crypto()->set_tpm(&tpm);
    homedirs_.crypto()->set_use_tpm(false);
    ASSERT_TRUE(homedirs_.GetSystemSalt(&system_salt_));
    set_policy(false, "", false, "");

    // Setup the base keyset files for users[1]
    keyset_paths_.push_back(test_helper_.users[1].keyset_path);
    keys_.push_back(test_helper_.users[1].passkey);

    MockFileEnumerator* files = new MockFileEnumerator();
    EXPECT_CALL(platform_, GetFileEnumerator(
        test_helper_.users[1].base_path, false, _))
      .WillOnce(Return(files));
    {
      InSequence s;
      // Single key.
      EXPECT_CALL(*files, Next())
        .WillOnce(Return(keyset_paths_[0]));
      EXPECT_CALL(*files, Next())
        .WillOnce(Return(""));
    }

    homedirs_.set_vault_keyset_factory(&vault_keyset_factory_);
    active_vk_ = new MockVaultKeyset();
    EXPECT_CALL(vault_keyset_factory_, New(_, _))
      .WillOnce(Return(active_vk_));
    EXPECT_CALL(*active_vk_, Load(keyset_paths_[0]))
      .WillOnce(Return(true));
    EXPECT_CALL(*active_vk_, Decrypt(_))
      .WillOnce(Invoke(this, &KeysetManagementTest::VkDecrypt0));

    cryptohome::SecureBlob passkey;
    cryptohome::Crypto::PasswordToPasskey(test_helper_.users[1].password,
                                          system_salt_, &passkey);
    up_.reset(new UsernamePasskey(test_helper_.users[1].username, passkey));
  }

  MockVaultKeyset* active_vk_;
  std::vector<std::string> keyset_paths_;
  std::vector<chromeos::SecureBlob> keys_;
  scoped_ptr<UsernamePasskey> up_;
  SecureBlob system_salt_;
};


TEST_F(KeysetManagementTest, AddKeysetSuccess) {
  KeysetSetUp();

  cryptohome::SecureBlob newkey;
  cryptohome::Crypto::PasswordToPasskey("why not", system_salt_, &newkey);
  int index = -1;
  // The injected keyset in the fixture handles the up_ validation.
  EXPECT_CALL(platform_, OpenFile(EndsWith("master.0"), StrEq("wx")))
    .WillOnce(Return(reinterpret_cast<FILE*>(NULL)));
  EXPECT_CALL(platform_, OpenFile(EndsWith("master.1"), StrEq("wx")))
    .WillOnce(Return(reinterpret_cast<FILE*>(0xbeefbeef)));
  EXPECT_CALL(*active_vk_, Encrypt(newkey))
    .WillOnce(Return(true));
  EXPECT_CALL(*active_vk_, Save(EndsWith("master.1")))
    .WillOnce(Return(true));
  EXPECT_CALL(platform_, DeleteFile(_, _))
    .Times(0);

  ASSERT_TRUE(homedirs_.AddKeyset(*up_, newkey, &index));
  EXPECT_EQ(index, 1);
}

TEST_F(KeysetManagementTest, AddKeysetInvalidCreds) {
  KeysetSetUp();

  cryptohome::SecureBlob newkey;
  cryptohome::Crypto::PasswordToPasskey("why not", system_salt_, &newkey);
  int index = -1;

  EXPECT_CALL(platform_, DeleteFile(_, _))
    .Times(0);
  // Try to authenticate with an unknown key.
  UsernamePasskey bad_p(test_helper_.users[1].username, newkey);
  ASSERT_FALSE(homedirs_.AddKeyset(bad_p, newkey, &index));
  EXPECT_EQ(index, -1);
}

TEST_F(KeysetManagementTest, AddKeyset0Available) {
  // While this doesn't affect the hole-finding logic, it's good to cover the
  // full logical behavior by changing which key auths too.
  // master.0 -> master.1
  test_helper_.users[1].keyset_path.erase(
    test_helper_.users[1].keyset_path.end() - 1);
  test_helper_.users[1].keyset_path.append("1");
  KeysetSetUp();

  // The injected keyset in the fixture handles the up_ validation.
  cryptohome::SecureBlob newkey;
  cryptohome::Crypto::PasswordToPasskey("why not", system_salt_, &newkey);

  EXPECT_CALL(platform_, OpenFile(EndsWith("master.0"), StrEq("wx")))
    .WillOnce(Return(reinterpret_cast<FILE*>(0xbeefbeef)));
  EXPECT_CALL(*active_vk_, Encrypt(newkey))
    .WillOnce(Return(true));
  EXPECT_CALL(*active_vk_, Save(EndsWith("master.0")))
    .WillOnce(Return(true));
  EXPECT_CALL(platform_, DeleteFile(_, _))
    .Times(0);

 int index = -1;
  // Try to authenticate with an unknown key.
  ASSERT_TRUE(homedirs_.AddKeyset(*up_, newkey, &index));
  EXPECT_EQ(index, 0);
}

TEST_F(KeysetManagementTest, AddKeyset10Available) {
  KeysetSetUp();

  // The injected keyset in the fixture handles the up_ validation.
  cryptohome::SecureBlob newkey;
  cryptohome::Crypto::PasswordToPasskey("why not", system_salt_, &newkey);

  EXPECT_CALL(platform_, OpenFile(MatchesRegex(".*/master\\..$"), StrEq("wx")))
    .Times(10)
    .WillRepeatedly(Return(reinterpret_cast<FILE*>(NULL)));
  EXPECT_CALL(platform_, OpenFile(EndsWith("master.10"), StrEq("wx")))
    .WillOnce(Return(reinterpret_cast<FILE*>(0xbeefbeef)));
  EXPECT_CALL(platform_, DeleteFile(_, _))
    .Times(0);
  EXPECT_CALL(*active_vk_, Encrypt(newkey))
    .WillOnce(Return(true));
  EXPECT_CALL(*active_vk_, Save(EndsWith("master.10")))
    .WillOnce(Return(true));

 int index = -1;
  // Try to authenticate with an unknown key.
  ASSERT_TRUE(homedirs_.AddKeyset(*up_, newkey, &index));
  EXPECT_EQ(index, 10);
}

TEST_F(KeysetManagementTest, AddKeysetNoFreeIndices) {
  KeysetSetUp();

  // The injected keyset in the fixture handles the up_ validation.
  cryptohome::SecureBlob newkey;
  cryptohome::Crypto::PasswordToPasskey("why not", system_salt_, &newkey);

  EXPECT_CALL(platform_, OpenFile(MatchesRegex(".*/master\\..*$"), StrEq("wx")))
    .Times(kKeyFileMax)
    .WillRepeatedly(Return(reinterpret_cast<FILE*>(NULL)));
  EXPECT_CALL(platform_, DeleteFile(_, _))
    .Times(0);

  int index = -1;
  // Try to authenticate with an unknown key.
  ASSERT_FALSE(homedirs_.AddKeyset(*up_, newkey, &index));
  EXPECT_EQ(index, -1);
}

TEST_F(KeysetManagementTest, AddKeysetEncryptFail) {
  KeysetSetUp();

  cryptohome::SecureBlob newkey;
  cryptohome::Crypto::PasswordToPasskey("why not", system_salt_, &newkey);
  int index = -1;
  // The injected keyset in the fixture handles the up_ validation.
  EXPECT_CALL(platform_, OpenFile(EndsWith("master.0"), StrEq("wx")))
    .WillOnce(Return(reinterpret_cast<FILE*>(0xbeefbeef)));
  EXPECT_CALL(*active_vk_, Encrypt(newkey))
    .WillOnce(Return(false));
  EXPECT_CALL(platform_, CloseFile(reinterpret_cast<FILE*>(0xbeefbeef)))
    .WillOnce(Return(true));
  EXPECT_CALL(platform_, DeleteFile(EndsWith("master.0"), false))
    .WillOnce(Return(true));
  ASSERT_FALSE(homedirs_.AddKeyset(*up_, newkey, &index));
  EXPECT_EQ(index, -1);
}

TEST_F(KeysetManagementTest, AddKeysetSaveFail) {
  KeysetSetUp();

  cryptohome::SecureBlob newkey;
  cryptohome::Crypto::PasswordToPasskey("why not", system_salt_, &newkey);
  int index = -1;
  // The injected keyset in the fixture handles the up_ validation.
  EXPECT_CALL(platform_, OpenFile(EndsWith("master.0"), StrEq("wx")))
    .WillOnce(Return(reinterpret_cast<FILE*>(0xbeefbeef)));
  EXPECT_CALL(*active_vk_, Encrypt(newkey))
    .WillOnce(Return(true));
  EXPECT_CALL(*active_vk_, Save(EndsWith("master.0")))
    .WillOnce(Return(false));
  EXPECT_CALL(platform_, CloseFile(reinterpret_cast<FILE*>(0xbeefbeef)))
    .WillOnce(Return(true));
  EXPECT_CALL(platform_, DeleteFile(EndsWith("master.0"), false))
    .WillOnce(Return(true));
  ASSERT_FALSE(homedirs_.AddKeyset(*up_, newkey, &index));
  EXPECT_EQ(index, -1);
}

TEST_F(KeysetManagementTest, ForceRemoveKeysetSuccess) {
  EXPECT_CALL(platform_, FileExists(EndsWith("master.0")))
    .WillOnce(Return(true));
  EXPECT_CALL(platform_, DeleteFile(EndsWith("master.0"), false))
    .WillOnce(Return(true));
  ASSERT_TRUE(homedirs_.ForceRemoveKeyset("a0b0c0", 0));
}

TEST_F(KeysetManagementTest, ForceRemoveKeysetMissingKeyset) {
  EXPECT_CALL(platform_, FileExists(EndsWith("master.0")))
    .WillOnce(Return(false));
  ASSERT_TRUE(homedirs_.ForceRemoveKeyset("a0b0c0", 0));
}

TEST_F(KeysetManagementTest, ForceRemoveKeysetNegativeIndex) {
  ASSERT_FALSE(homedirs_.ForceRemoveKeyset("a0b0c0", -1));
}

TEST_F(KeysetManagementTest, ForceRemoveKeysetOverMaxIndex) {
  ASSERT_FALSE(homedirs_.ForceRemoveKeyset("a0b0c0", kKeyFileMax));
}

TEST_F(KeysetManagementTest, ForceRemoveKeysetFailedDelete) {
  EXPECT_CALL(platform_, FileExists(EndsWith("master.0")))
    .WillOnce(Return(true));
  EXPECT_CALL(platform_, DeleteFile(EndsWith("master.0"), false))
    .WillOnce(Return(false));
  ASSERT_FALSE(homedirs_.ForceRemoveKeyset("a0b0c0", 0));
}

TEST_F(KeysetManagementTest, MoveKeysetSuccess_0_to_1) {
  const std::string obfuscated = "a0b0c0";
  EXPECT_CALL(platform_, FileExists(EndsWith("master.0")))
    .WillOnce(Return(true));
  EXPECT_CALL(platform_, FileExists(EndsWith("master.1")))
    .WillOnce(Return(false));
  EXPECT_CALL(platform_, OpenFile(EndsWith("master.1"), StrEq("wx")))
    .WillOnce(Return(reinterpret_cast<FILE*>(0xbeefbeef)));
  EXPECT_CALL(platform_, Rename(EndsWith("master.0"), EndsWith("master.1")))
    .WillOnce(Return(true));
  EXPECT_CALL(platform_, CloseFile(reinterpret_cast<FILE*>(0xbeefbeef)))
    .WillOnce(Return(true));
  ASSERT_TRUE(homedirs_.MoveKeyset(obfuscated, 0, 1));
}

TEST_F(KeysetManagementTest, MoveKeysetSuccess_1_to_99) {
  const std::string obfuscated = "a0b0c0";
  EXPECT_CALL(platform_, FileExists(EndsWith("master.1")))
    .WillOnce(Return(true));
  EXPECT_CALL(platform_, FileExists(EndsWith("master.99")))
    .WillOnce(Return(false));
  EXPECT_CALL(platform_, OpenFile(EndsWith("master.99"), StrEq("wx")))
    .WillOnce(Return(reinterpret_cast<FILE*>(0xbeefbeef)));
  EXPECT_CALL(platform_, Rename(EndsWith("master.1"), EndsWith("master.99")))
    .WillOnce(Return(true));
  EXPECT_CALL(platform_, CloseFile(reinterpret_cast<FILE*>(0xbeefbeef)))
    .WillOnce(Return(true));
  ASSERT_TRUE(homedirs_.MoveKeyset(obfuscated, 1, 99));
}

TEST_F(KeysetManagementTest, MoveKeysetNegativeSource) {
  const std::string obfuscated = "a0b0c0";
  ASSERT_FALSE(homedirs_.MoveKeyset(obfuscated, -1, 1));
}

TEST_F(KeysetManagementTest, MoveKeysetNegativeDestination) {
  const std::string obfuscated = "a0b0c0";
  ASSERT_FALSE(homedirs_.MoveKeyset(obfuscated, 1, -1));
}

TEST_F(KeysetManagementTest, MoveKeysetTooLargeDestination) {
  const std::string obfuscated = "a0b0c0";
  ASSERT_FALSE(homedirs_.MoveKeyset(obfuscated, 1, kKeyFileMax));
}

TEST_F(KeysetManagementTest, MoveKeysetTooLargeSource) {
  const std::string obfuscated = "a0b0c0";
  ASSERT_FALSE(homedirs_.MoveKeyset(obfuscated, kKeyFileMax, 0));
}

TEST_F(KeysetManagementTest, MoveKeysetMissingSource) {
  const std::string obfuscated = "a0b0c0";
  EXPECT_CALL(platform_, FileExists(EndsWith("master.0")))
    .WillOnce(Return(false));
  ASSERT_FALSE(homedirs_.MoveKeyset(obfuscated, 0, 1));
}

TEST_F(KeysetManagementTest, MoveKeysetDestinationExists) {
  const std::string obfuscated = "a0b0c0";
  EXPECT_CALL(platform_, FileExists(EndsWith("master.0")))
    .WillOnce(Return(true));
  EXPECT_CALL(platform_, FileExists(EndsWith("master.1")))
    .WillOnce(Return(true));
  ASSERT_FALSE(homedirs_.MoveKeyset(obfuscated, 0, 1));
}

TEST_F(KeysetManagementTest, MoveKeysetExclusiveOpenFailed) {
  const std::string obfuscated = "a0b0c0";
  EXPECT_CALL(platform_, FileExists(EndsWith("master.0")))
    .WillOnce(Return(true));
  EXPECT_CALL(platform_, FileExists(EndsWith("master.1")))
    .WillOnce(Return(false));
  EXPECT_CALL(platform_, OpenFile(EndsWith("master.1"), StrEq("wx")))
    .WillOnce(Return(reinterpret_cast<FILE*>(NULL)));
  ASSERT_FALSE(homedirs_.MoveKeyset(obfuscated, 0, 1));
}

TEST_F(KeysetManagementTest, MoveKeysetRenameFailed) {
  const std::string obfuscated = "a0b0c0";
  EXPECT_CALL(platform_, FileExists(EndsWith("master.0")))
    .WillOnce(Return(true));
  EXPECT_CALL(platform_, FileExists(EndsWith("master.1")))
    .WillOnce(Return(false));
  EXPECT_CALL(platform_, OpenFile(EndsWith("master.1"), StrEq("wx")))
    .WillOnce(Return(reinterpret_cast<FILE*>(0xbeefbeef)));
  EXPECT_CALL(platform_, Rename(EndsWith("master.0"), EndsWith("master.1")))
    .WillOnce(Return(false));
  EXPECT_CALL(platform_, CloseFile(reinterpret_cast<FILE*>(0xbeefbeef)))
    .WillOnce(Return(true));
  ASSERT_FALSE(homedirs_.MoveKeyset(obfuscated, 0, 1));
}

}  // namespace cryptohome
