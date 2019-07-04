// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/userdataauth.h"

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <chaps/token_manager_client_mock.h>
#include <vector>

#include "cryptohome/mock_crypto.h"
#include "cryptohome/mock_homedirs.h"
#include "cryptohome/mock_mount.h"
#include "cryptohome/mock_mount_factory.h"
#include "cryptohome/mock_platform.h"
#include "cryptohome/mock_tpm.h"
#include "cryptohome/mock_tpm_init.h"

using base::FilePath;
using brillo::SecureBlob;

using ::testing::_;
using ::testing::EndsWith;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::WithArgs;

namespace cryptohome {

namespace {

bool AssignSalt(size_t size, SecureBlob* salt) {
  SecureBlob fake_salt(size, 'S');
  salt->swap(fake_salt);
  return true;
}

}  // namespace

// UserDataAuthTestNotInitialized is a test fixture that does not call
// UserDataAuth::Initialize() during setup. Therefore, it's suited to tests that
// can be conducted without calling UserDataAuth::Initialize(), or for tests
// that wants some flexibility before calling UserDataAuth::Initialize(), note
// that in this case the test have to call UserDataAuth::Initialize().
class UserDataAuthTestNotInitialized : public ::testing::Test {
 public:
  UserDataAuthTestNotInitialized() = default;
  ~UserDataAuthTestNotInitialized() override = default;

  void SetUp() override {
    tpm_init_.set_tpm(&tpm_);

    userdataauth_.set_crypto(&crypto_);
    userdataauth_.set_homedirs(&homedirs_);
    userdataauth_.set_tpm(&tpm_);
    userdataauth_.set_tpm_init(&tpm_init_);
    userdataauth_.set_platform(&platform_);
    userdataauth_.set_chaps_client(&chaps_client_);
    userdataauth_.set_disable_threading(true);
    homedirs_.set_crypto(&crypto_);
    homedirs_.set_platform(&platform_);
    ON_CALL(homedirs_, Init(_, _, _)).WillByDefault(Return(true));
    // Empty token list by default.  The effect is that there are no attempts
    // to unload tokens unless a test explicitly sets up the token list.
    ON_CALL(chaps_client_, GetTokenList(_, _)).WillByDefault(Return(true));
    // Skip CleanUpStaleMounts by default.
    ON_CALL(platform_, GetMountsBySourcePrefix(_, _))
        .WillByDefault(Return(false));
    // Setup fake salt by default.
    ON_CALL(crypto_, GetOrCreateSalt(_, _, _, _))
        .WillByDefault(WithArgs<1, 3>(Invoke(AssignSalt)));
  }

  // This is a utility function for tests to setup a mount for a particular
  // user. After calling this function, |mount_| is available for use.
  void SetupMount(const std::string& username) {
    mount_ = new NiceMock<MockMount>();
    userdataauth_.set_mount_for_user(username, mount_.get());
  }

 protected:
  // Mock Crypto object, will be passed to UserDataAuth for its internal use.
  NiceMock<MockCrypto> crypto_;

  // Mock HomeDirs object, will be passed to UserDataAuth for its internal use.
  NiceMock<MockHomeDirs> homedirs_;

  // Mock Platform object, will be passed to UserDataAuth for its internal use.
  NiceMock<MockPlatform> platform_;

  // Mock TPM object, will be passed to UserDataAuth for its internal use.
  NiceMock<MockTpm> tpm_;

  // Mock TPM Init object, will be passed to UserDataAuth for its internal use.
  NiceMock<MockTpmInit> tpm_init_;

  // Mock chaps token manager client, will be passed to UserDataAuth for its
  // internal use.
  NiceMock<chaps::TokenManagerClientMock> chaps_client_;

  // This is used to hold the mount object when we create a mock mount with
  // SetupMount().
  scoped_refptr<MockMount> mount_;

  // Declare |userdataauth_| last so it gets destroyed before all the mocks.
  // This is important because otherwise the background thread may call into
  // mocks that have already been destroyed.
  UserDataAuth userdataauth_;

 private:
  DISALLOW_COPY_AND_ASSIGN(UserDataAuthTestNotInitialized);
};

// Standard, fully initialized UserDataAuth test fixture
class UserDataAuthTest : public UserDataAuthTestNotInitialized {
 public:
  UserDataAuthTest() = default;
  ~UserDataAuthTest() override = default;

  void SetUp() override {
    UserDataAuthTestNotInitialized::SetUp();
    ASSERT_TRUE(userdataauth_.Initialize());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(UserDataAuthTest);
};

TEST_F(UserDataAuthTest, IsMounted) {
  // By default there are no mount right after initialization
  EXPECT_FALSE(userdataauth_.IsMounted());
  EXPECT_FALSE(userdataauth_.IsMounted("foo@gmail.com"));

  // Add a mount associated with foo@gmail.com
  SetupMount("foo@gmail.com");

  // Test the code path that doesn't specify a user, and when there's a mount
  // that's unmounted.
  EXPECT_CALL(*mount_, IsMounted()).WillOnce(Return(false));
  EXPECT_FALSE(userdataauth_.IsMounted());

  // Test to see if is_ephemeral works and test the code path that doesn't
  // specify a user.
  bool is_ephemeral = true;
  EXPECT_CALL(*mount_, IsMounted()).WillOnce(Return(true));
  EXPECT_CALL(*mount_, IsNonEphemeralMounted()).WillOnce(Return(true));
  EXPECT_TRUE(userdataauth_.IsMounted("", &is_ephemeral));
  EXPECT_FALSE(is_ephemeral);

  // Test to see if is_ephemeral works, and test the code path that specify the
  // user.
  EXPECT_CALL(*mount_, IsMounted()).WillOnce(Return(true));
  EXPECT_CALL(*mount_, IsNonEphemeralMounted()).WillOnce(Return(false));
  EXPECT_TRUE(userdataauth_.IsMounted("foo@gmail.com", &is_ephemeral));
  EXPECT_TRUE(is_ephemeral);

  // Note: IsMounted will not be called in this case.
  EXPECT_FALSE(userdataauth_.IsMounted("bar@gmail.com", &is_ephemeral));
  EXPECT_FALSE(is_ephemeral);
}

TEST_F(UserDataAuthTest, Unmount) {
  // Unmount sanity test.
  // The tests on whether stale mount are cleaned up is in another set of tests
  // called CleanUpStale_*

  // Add a mount associated with foo@gmail.com
  SetupMount("foo@gmail.com");

  // Unmount will be successful.
  EXPECT_CALL(*mount_, UnmountCryptohome()).WillOnce(Return(true));
  // If anyone asks, this mount is still mounted.
  ON_CALL(*mount_, IsMounted()).WillByDefault(Return(true));

  // Unmount should be successful.
  EXPECT_TRUE(userdataauth_.Unmount());

  // It should be unmounted in the end.
  EXPECT_FALSE(userdataauth_.IsMounted());

  // Add another mount associated with bar@gmail.com
  SetupMount("bar@gmail.com");

  // Unmount will be unsuccessful.
  EXPECT_CALL(*mount_, UnmountCryptohome()).WillOnce(Return(false));
  // If anyone asks, this mount is still mounted.
  ON_CALL(*mount_, IsMounted()).WillByDefault(Return(true));

  // Unmount should be honest about failures.
  EXPECT_FALSE(userdataauth_.Unmount());

  // Unmount will remove all mounts even if it failed.
  EXPECT_FALSE(userdataauth_.IsMounted());
}

// ======================= CleanUpStaleMounts tests ==========================

namespace {

struct Mounts {
  const FilePath src;
  const FilePath dst;
};

const Mounts kShadowMounts[] = {
    {FilePath("/home/.shadow/a"), FilePath("/home/user/0")},
    {FilePath("/home/.shadow/a"), FilePath("/home/root/0")},
    {FilePath("/home/.shadow/b"), FilePath("/home/user/1")},
    {FilePath("/home/.shadow/a"), FilePath("/home/chronos/user")},
    {FilePath("/home/.shadow/b"), FilePath("/home/root/1")},
    {FilePath("/home/user/b/Downloads"),
     FilePath("/home/user/b/MyFiles/Downloads")},
    {FilePath("/home/chronos/u-b/Downloads"),
     FilePath("/home/chronos/u-b/MyFiles/Downloads")},
    {FilePath("/home/chronos/user/Downloads"),
     FilePath("/home/chronos/user/MyFiles/Downloads")},
};

const int kShadowMountsCount = 8;

const Mounts kLoopDevMounts[] = {
    {FilePath("/dev/loop7"), FilePath("/run/cryptohome/ephemeral_mount/1")},
    {FilePath("/dev/loop7"), FilePath("/home/user/0")},
    {FilePath("/dev/loop7"), FilePath("/home/root/0")},
    {FilePath("/dev/loop7"), FilePath("/home/chronos/u-1")},
    {FilePath("/dev/loop7"), FilePath("/home/chronos/user")},
    {FilePath("/dev/loop1"), FilePath("/opt/google/containers")},
    {FilePath("/dev/loop2"), FilePath("/home/root/1")},
    {FilePath("/dev/loop2"), FilePath("/home/user/1")},
};

// 5 Mounts in the above are from /dev/loop7, which is ephemeral as seen
// in kLoopDevices.
const int kEphemeralMountsCount = 5;

// Constants used by CleanUpStaleMounts tests.
const Platform::LoopDevice kLoopDevices[] = {
    {FilePath("/mnt/stateful_partition/encrypted.block"),
     FilePath("/dev/loop0")},
    {FilePath("/run/cryptohome/ephemeral_data/1"), FilePath("/dev/loop7")},
};

const FilePath kSparseFiles[] = {
    FilePath("/run/cryptohome/ephemeral_data/2"),
    FilePath("/run/cryptohome/ephemeral_data/1"),
};

// Utility functions used by CleanUpStaleMounts tests.
bool StaleShadowMounts(const FilePath& from_prefix,
                       std::multimap<const FilePath, const FilePath>* mounts) {
  if (from_prefix.value() == "/home/.shadow") {
    if (!mounts)
      return true;
    const struct Mounts* m = &kShadowMounts[0];
    for (int i = 0; i < kShadowMountsCount; ++i, ++m) {
      mounts->insert(std::pair<const FilePath, const FilePath>(m->src, m->dst));
    }
    return true;
  }
  return false;
}

bool LoopDeviceMounts(std::multimap<const FilePath, const FilePath>* mounts) {
  if (!mounts)
    return false;
  const Mounts* m = kLoopDevMounts;
  for (int i = 0; i < arraysize(kLoopDevMounts); ++i, ++m)
    mounts->insert(std::make_pair(m->src, m->dst));
  return true;
}

bool EnumerateSparseFiles(const base::FilePath& path,
                          bool is_recursive,
                          std::vector<base::FilePath>* ent_list) {
  if (path != FilePath(kEphemeralCryptohomeDir).Append(kSparseFileDir))
    return false;
  ent_list->insert(ent_list->end(), std::begin(kSparseFiles),
                   std::end(kSparseFiles));
  return true;
}

}  // namespace

TEST_F(UserDataAuthTest, CleanUpStale_NoOpenFiles_Ephemeral) {
  // Check that when we have ephemeral mounts, no active mounts,
  // and no open filehandles, all stale mounts are unmounted, loop device is
  // detached and sparse file is deleted.

  EXPECT_CALL(platform_, GetMountsBySourcePrefix(homedirs_.shadow_root(), _))
      .WillOnce(Return(false));
  EXPECT_CALL(platform_, GetAttachedLoopDevices())
      .WillRepeatedly(Return(std::vector<Platform::LoopDevice>(
          std::begin(kLoopDevices), std::end(kLoopDevices))));
  EXPECT_CALL(platform_, GetLoopDeviceMounts(_))
      .WillOnce(Invoke(LoopDeviceMounts));
  EXPECT_CALL(
      platform_,
      EnumerateDirectoryEntries(
          FilePath(kEphemeralCryptohomeDir).Append(kSparseFileDir), _, _))
      .WillOnce(Invoke(EnumerateSparseFiles));
  EXPECT_CALL(platform_, GetProcessesWithOpenFiles(_, _))
      .Times(kEphemeralMountsCount);

  for (int i = 0; i < kEphemeralMountsCount; ++i) {
    EXPECT_CALL(platform_, Unmount(kLoopDevMounts[i].dst, true, _))
        .WillRepeatedly(Return(true));
  }
  EXPECT_CALL(platform_, DetachLoop(FilePath("/dev/loop7")))
      .WillOnce(Return(true));
  EXPECT_CALL(platform_, DeleteFile(kSparseFiles[0], _)).WillOnce(Return(true));
  EXPECT_CALL(platform_, DeleteFile(kSparseFiles[1], _)).WillOnce(Return(true));
  EXPECT_CALL(platform_, DeleteFile(kLoopDevMounts[0].dst, _))
      .WillOnce(Return(true));
  EXPECT_FALSE(userdataauth_.CleanUpStaleMounts(false));
}

TEST_F(UserDataAuthTest, CleanUpStale_OpenLegacy_Ephemeral) {
  // Check that when we have ephemeral mounts, no active mounts,
  // and some open filehandles to the legacy homedir, everything is kept.

  EXPECT_CALL(platform_, GetMountsBySourcePrefix(homedirs_.shadow_root(), _))
      .WillOnce(Return(false));
  EXPECT_CALL(platform_, GetAttachedLoopDevices())
      .WillRepeatedly(Return(std::vector<Platform::LoopDevice>(
          std::begin(kLoopDevices), std::end(kLoopDevices))));
  EXPECT_CALL(platform_, GetLoopDeviceMounts(_))
      .WillOnce(Invoke(LoopDeviceMounts));
  EXPECT_CALL(
      platform_,
      EnumerateDirectoryEntries(
          FilePath(kEphemeralCryptohomeDir).Append(kSparseFileDir), _, _))
      .WillOnce(Invoke(EnumerateSparseFiles));
  EXPECT_CALL(platform_, GetProcessesWithOpenFiles(_, _))
      .Times(kEphemeralMountsCount - 1);
  std::vector<ProcessInformation> processes(1);
  processes[0].set_process_id(1);
  EXPECT_CALL(platform_,
              GetProcessesWithOpenFiles(FilePath("/home/chronos/user"), _))
      .Times(1)
      .WillRepeatedly(SetArgPointee<1>(processes));

  EXPECT_CALL(platform_, Unmount(_, _, _)).Times(0);
  EXPECT_TRUE(userdataauth_.CleanUpStaleMounts(false));
}

TEST_F(UserDataAuthTest, CleanUpStale_OpenLegacy_Ephemeral_Forced) {
  // Check that when we have ephemeral mounts, no active mounts,
  // and some open filehandles to the legacy homedir, but cleanup is forced,
  // all mounts are unmounted, loop device is detached and file is deleted.

  EXPECT_CALL(platform_, GetMountsBySourcePrefix(homedirs_.shadow_root(), _))
      .WillOnce(Return(false));
  EXPECT_CALL(platform_, GetAttachedLoopDevices())
      .WillRepeatedly(Return(std::vector<Platform::LoopDevice>(
          std::begin(kLoopDevices), std::end(kLoopDevices))));
  EXPECT_CALL(platform_, GetLoopDeviceMounts(_))
      .WillOnce(Invoke(LoopDeviceMounts));
  EXPECT_CALL(
      platform_,
      EnumerateDirectoryEntries(
          FilePath(kEphemeralCryptohomeDir).Append(kSparseFileDir), _, _))
      .WillOnce(Invoke(EnumerateSparseFiles));
  EXPECT_CALL(platform_, GetProcessesWithOpenFiles(_, _)).Times(0);

  for (int i = 0; i < kEphemeralMountsCount; ++i) {
    EXPECT_CALL(platform_, Unmount(kLoopDevMounts[i].dst, true, _))
        .WillRepeatedly(Return(true));
  }
  EXPECT_CALL(platform_, DetachLoop(FilePath("/dev/loop7")))
      .WillOnce(Return(true));
  EXPECT_CALL(platform_, DeleteFile(kSparseFiles[0], _)).WillOnce(Return(true));
  EXPECT_CALL(platform_, DeleteFile(kSparseFiles[1], _)).WillOnce(Return(true));
  EXPECT_CALL(platform_, DeleteFile(kLoopDevMounts[0].dst, _))
      .WillOnce(Return(true));
  EXPECT_FALSE(userdataauth_.CleanUpStaleMounts(true));
}

TEST_F(UserDataAuthTest, CleanUpStale_EmptyMap_NoOpenFiles_ShadowOnly) {
  // Check that when we have a bunch of stale shadow mounts, no active mounts,
  // and no open filehandles, all stale mounts are unmounted.

  EXPECT_CALL(platform_, GetMountsBySourcePrefix(_, _))
      .WillOnce(Invoke(StaleShadowMounts));
  EXPECT_CALL(platform_, GetAttachedLoopDevices())
      .WillRepeatedly(Return(std::vector<Platform::LoopDevice>()));
  EXPECT_CALL(platform_, GetLoopDeviceMounts(_)).WillOnce(Return(false));
  EXPECT_CALL(
      platform_,
      EnumerateDirectoryEntries(
          FilePath(kEphemeralCryptohomeDir).Append(kSparseFileDir), _, _))
      .WillOnce(Return(false));
  EXPECT_CALL(platform_, GetProcessesWithOpenFiles(_, _))
      .Times(kShadowMountsCount);
  EXPECT_CALL(platform_, Unmount(_, true, _))
      .Times(kShadowMountsCount)
      .WillRepeatedly(Return(true));
  EXPECT_FALSE(userdataauth_.CleanUpStaleMounts(false));
}

TEST_F(UserDataAuthTest, CleanUpStale_EmptyMap_OpenLegacy_ShadowOnly) {
  // Check that when we have a bunch of stale shadow mounts, no active mounts,
  // and some open filehandles to the legacy homedir, all mounts without
  // filehandles are unmounted.

  EXPECT_CALL(platform_, GetMountsBySourcePrefix(_, _))
      .WillOnce(Invoke(StaleShadowMounts));
  EXPECT_CALL(platform_, GetAttachedLoopDevices())
      .WillRepeatedly(Return(std::vector<Platform::LoopDevice>()));
  EXPECT_CALL(platform_, GetLoopDeviceMounts(_)).WillOnce(Return(false));
  EXPECT_CALL(
      platform_,
      EnumerateDirectoryEntries(
          FilePath(kEphemeralCryptohomeDir).Append(kSparseFileDir), _, _))
      .WillOnce(Return(false));
  std::vector<ProcessInformation> processes(1);
  processes[0].set_process_id(1);
  EXPECT_CALL(platform_, GetProcessesWithOpenFiles(_, _))
      .Times(kShadowMountsCount - 1);
  EXPECT_CALL(platform_,
              GetProcessesWithOpenFiles(FilePath("/home/chronos/user"), _))
      .Times(1)
      .WillRepeatedly(SetArgPointee<1>(processes));
  EXPECT_CALL(
      platform_,
      Unmount(Property(&FilePath::value,
                       AnyOf(EndsWith("/1"), EndsWith("/MyFiles/Downloads"))),
              true, _))
      .Times(5)
      .WillRepeatedly(Return(true));
  EXPECT_TRUE(userdataauth_.CleanUpStaleMounts(false));
}

}  // namespace cryptohome
