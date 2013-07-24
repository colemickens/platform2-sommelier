// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for Service

#include "service.h"

#include <base/at_exit.h>
#include <base/threading/platform_thread.h>
#include <base/file_util.h>
#include <base/time.h>
#include <chromeos/cryptohome.h>
#include <chromeos/secure_blob.h>
#include <gtest/gtest.h>
#include <policy/libpolicy.h>
#include <policy/mock_device_policy.h>

#include "crypto.h"
#include "make_tests.h"
#include "mock_homedirs.h"
#include "mock_install_attributes.h"
#include "mock_mount.h"
#include "mock_mount_factory.h"
#include "mock_platform.h"
#include "mock_tpm.h"
#include "username_passkey.h"

using base::PlatformThread;
using chromeos::SecureBlob;

namespace cryptohome {
using ::testing::_;
using ::testing::EndsWith;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StartsWith;
using ::testing::SetArgumentPointee;

const char kImageDir[] = "test_image_dir";
const char kSkelDir[] = "test_image_dir/skel";
const char kSaltFile[] = "test_image_dir/salt";
class ServiceInterfaceTest : public ::testing::Test {
 public:
  ServiceInterfaceTest() { }
  virtual ~ServiceInterfaceTest() { }

  void SetUp() {
    test_helper_.SetUpSystemSalt();
  }
  void TearDown() {
    test_helper_.TearDownSystemSalt();
  }

 protected:
  MakeTests test_helper_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceInterfaceTest);
};

class ServiceSubclass : public Service {
 public:
  ServiceSubclass()
    : Service(),
      completed_tasks_() { }
  virtual ~ServiceSubclass() { }

  virtual void NotifyEvent(CryptohomeEventBase* result) {
    if (strcmp(result->GetEventName(), kMountTaskResultEventType))
      return;
    MountTaskResult* r = static_cast<MountTaskResult*>(result);
    completed_tasks_.push_back(*r);
  }

  virtual void Dispatch() {
    DispatchEvents();
  }

  std::vector<MountTaskResult> completed_tasks_;
};

TEST_F(ServiceInterfaceTest, CheckKeySuccessTest) {
  MockHomeDirs homedirs;
  scoped_refptr<MockMount> mount = new MockMount();
  EXPECT_CALL(homedirs, Init())
      .WillOnce(Return(true));
  EXPECT_CALL(homedirs, FreeDiskSpace())
      .WillOnce(Return(true));
  EXPECT_CALL(*mount, AreSameUser(_))
      .WillOnce(Return(false));
  EXPECT_CALL(homedirs, AreCredentialsValid(_))
      .WillOnce(Return(true));

  Service service;
  service.set_homedirs(&homedirs);
  service.set_mount_for_user("chromeos-user", mount.get());
  NiceMock<MockInstallAttributes> attrs;
  service.set_install_attrs(&attrs);
  service.set_initialize_tpm(false);
  service.Initialize();
  gboolean out = FALSE;
  GError *error = NULL;

  char user[] = "chromeos-user";
  char key[] = "274146c6e8886a843ddfea373e2dc71b";
  EXPECT_TRUE(service.CheckKey(user, key, &out, &error));
  EXPECT_TRUE(out);
}

TEST_F(ServiceInterfaceTest, CheckAsyncTestCredentials) {
  NiceMock<MockTpm> tpm;
  NiceMock<MockPlatform> platform;

  test_helper_.InjectSystemSalt(&platform, kSaltFile);
  test_helper_.InitTestData(kImageDir, kDefaultUsers, kDefaultUserCount);
  TestUser* user = &test_helper_.users[7];
  user->InjectKeyset(&platform);

  HomeDirs homedirs;
  homedirs.crypto()->set_tpm(&tpm);
  homedirs.crypto()->set_use_tpm(false);
  homedirs.crypto()->set_platform(&platform);
  homedirs.set_shadow_root(kImageDir);
  homedirs.set_platform(&platform);
  homedirs.set_policy_provider(new policy::PolicyProvider(
      new NiceMock<policy::MockDevicePolicy>));

  ServiceSubclass service;
  service.set_platform(&platform);
  service.set_homedirs(&homedirs);
  service.crypto()->set_platform(&platform);
  NiceMock<MockInstallAttributes> attrs;
  service.set_install_attrs(&attrs);
  service.set_initialize_tpm(false);
  service.Initialize();

  SecureBlob passkey;
  cryptohome::Crypto::PasswordToPasskey(user->password,
                                        test_helper_.system_salt, &passkey);
  std::string passkey_string(static_cast<const char*>(passkey.const_data()),
                             passkey.size());

  gboolean out = FALSE;
  GError *error = NULL;
  gint async_id = -1;
  EXPECT_TRUE(service.AsyncCheckKey(
      const_cast<gchar*>(static_cast<const gchar*>(user->username)),
      const_cast<gchar*>(static_cast<const gchar*>(passkey_string.c_str())),
      &async_id,
      &error));
  EXPECT_NE(-1, async_id);
  for (unsigned int i = 0; i < 64; i++) {
    bool found = false;
    service.Dispatch();
    for (unsigned int j = 0; j < service.completed_tasks_.size(); j++) {
      if (service.completed_tasks_[j].sequence_id() == async_id) {
        out = service.completed_tasks_[j].return_status();
        found = true;
      }
    }
    if (found) {
      break;
    }
    PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(100));
  }
  EXPECT_TRUE(out);
}

TEST_F(ServiceInterfaceTest, GetPublicMountPassKey) {
  NiceMock<MockPlatform> platform;

  const char kPublicMountSaltPath[] = "/var/lib/public_mount_salt";
  chromeos::Blob public_mount_salt(CRYPTOHOME_DEFAULT_SALT_LENGTH, 'P');
  EXPECT_CALL(platform, FileExists(kPublicMountSaltPath))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(platform, GetFileSize(kPublicMountSaltPath, _))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(public_mount_salt.size()),
                          Return(true)));
  EXPECT_CALL(platform, ReadFile(kPublicMountSaltPath, _))
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(public_mount_salt),
                          Return(true)));

  MockHomeDirs homedirs;
  ServiceSubclass service;
  service.set_platform(&platform);
  service.set_homedirs(&homedirs);
  service.crypto()->set_platform(&platform);
  NiceMock<MockInstallAttributes> attrs;
  service.set_install_attrs(&attrs);
  service.set_initialize_tpm(false);
  service.Initialize();

  const char kPublicUser1[] = "public_user_1";
  const char kPublicUser2[] = "public_user_2";

  std::string public_user1_passkey;
  service.GetPublicMountPassKey(kPublicUser1, &public_user1_passkey);

  std::string public_user2_passkey;
  service.GetPublicMountPassKey(kPublicUser2, &public_user2_passkey);
  // The passkey should be different for different user.
  EXPECT_NE(public_user1_passkey, public_user2_passkey);

  std::string public_user1_passkey2;
  service.GetPublicMountPassKey(kPublicUser1, &public_user1_passkey2);
  // The passkey should be the same for the same user.
  EXPECT_EQ(public_user1_passkey, public_user1_passkey2);
}

TEST_F(ServiceInterfaceTest, GetSanitizedUsername) {
  Service service;
  char username[] = "chromeos-user";
  gchar *sanitized = NULL;
  GError *error = NULL;
  EXPECT_TRUE(service.GetSanitizedUsername(username, &sanitized, &error));
  EXPECT_TRUE(error == NULL);
  ASSERT_TRUE(sanitized);

  const std::string expected(
      chromeos::cryptohome::home::SanitizeUserName(username));
  EXPECT_FALSE(expected.empty());

  EXPECT_EQ(expected, sanitized);
  g_free(sanitized);
}

TEST(Standalone, CheckAutoCleanupCallback) {
  // Checks that AutoCleanupCallback() is called periodically.
  MockHomeDirs homedirs;
  Service service;
  service.set_homedirs(&homedirs);
  NiceMock<MockInstallAttributes> attrs;
  service.set_install_attrs(&attrs);
  service.set_initialize_tpm(false);

  // Service will schedule periodic clean-ups. Wait a bit and make
  // sure that we had at least 3 executed.
  EXPECT_CALL(homedirs, Init())
      .WillOnce(Return(true));
  EXPECT_CALL(homedirs, FreeDiskSpace())
      .Times(::testing::AtLeast(3));

  scoped_refptr<MockMount> mount = new MockMount();
  EXPECT_CALL(*mount, UpdateCurrentUserActivityTimestamp(0))
      .Times(::testing::AtLeast(3));
  service.set_mount_for_user("some-user-to-clean-up", mount.get());

  service.set_auto_cleanup_period(2);  // 2ms = 500HZ
  service.set_update_user_activity_period(2);  // 2 x 5ms = 25HZ
  service.Initialize();
  PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(100));
}

TEST(Standalone, CheckAutoCleanupCallbackFirst) {
  // Checks that AutoCleanupCallback() is called first right after init.
  MockHomeDirs homedirs;
  Service service;
  service.set_homedirs(&homedirs);
  NiceMock<MockInstallAttributes> attrs;
  service.set_install_attrs(&attrs);
  service.set_initialize_tpm(false);

  // Service will schedule first cleanup right after its init.
  EXPECT_CALL(homedirs, Init())
      .WillOnce(Return(true));
  EXPECT_CALL(homedirs, FreeDiskSpace())
      .Times(1);
  service.set_auto_cleanup_period(1000);  // 1s - long enough
  service.Initialize();
  // short delay to see the first invocation
  PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(10));
}

struct Mounts {
  const char* src;
  const char* dst;
};

const struct Mounts kShadowMounts[] = {
  { "/home/.shadow/a", "/home/user/0" },
  { "/home/.shadow/a", "/home/root/0" },
  { "/home/.shadow/b", "/home/user/1" },
  { "/home/.shadow/a", "/home/chronos/user" },
  { "/home/.shadow/b", "/home/root/1" },
};
const int kShadowMountsCount = 5;

bool StaleShadowMounts(const std::string& from_prefix,
                 std::multimap<const std::string, const std::string>* mounts) {
  LOG(INFO) << "StaleShadowMounts(" << from_prefix << "): called";
  if (from_prefix == "/home/.shadow/") {
    if (!mounts)
      return true;
    const struct Mounts* m = &kShadowMounts[0];
    for (int i = 0; i < kShadowMountsCount; ++i, ++m) {
      mounts->insert(
          std::pair<const std::string, const std::string>(m->src, m->dst));
      LOG(INFO) << "Inserting " << m->src << ":" << m->dst;
    }
    return true;
  }
  return false;
}

class CleanUpStaleTest : public ::testing::Test {
 public:
  CleanUpStaleTest() { }
  virtual ~CleanUpStaleTest() { }

  void SetUp() {
    service_.set_homedirs(&homedirs_);
    service_.set_install_attrs(&attrs_);
    service_.set_initialize_tpm(false);
    service_.set_platform(&platform_);
  }

  void TearDown() { }

 protected:
  NiceMock<MockHomeDirs> homedirs_;
  NiceMock<MockInstallAttributes> attrs_;
  MockPlatform platform_;
  Service service_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CleanUpStaleTest);
};

TEST_F(CleanUpStaleTest, EmptyMap_NoOpenFiles_ShadowOnly) {
  // Checks that AutoCleanupCallback() is called first right after init.

  EXPECT_CALL(platform_, GetMountsBySourcePrefix(_, _))
    .Times(3)
    .WillRepeatedly(Invoke(StaleShadowMounts));
  EXPECT_CALL(platform_, GetProcessesWithOpenFiles(_, _))
    .Times(kShadowMountsCount);
  EXPECT_CALL(platform_, Unmount(_, true, _))
    .Times(kShadowMountsCount)
    .WillRepeatedly(Return(true));
  EXPECT_FALSE(service_.CleanUpStaleMounts(false));
}

TEST_F(CleanUpStaleTest, EmptyMap_OpenLegacy_ShadowOnly) {
  // Checks that AutoCleanupCallback() is called first right after init.
  EXPECT_CALL(platform_, GetMountsBySourcePrefix(_, _))
    .Times(3)
    .WillRepeatedly(Invoke(StaleShadowMounts));
  std::vector<ProcessInformation> processes(1);
  processes[0].set_process_id(1);
  EXPECT_CALL(platform_, GetProcessesWithOpenFiles(_, _))
    .Times(kShadowMountsCount - 1);
  EXPECT_CALL(platform_, GetProcessesWithOpenFiles(
      "/home/chronos/user", _))
    .Times(1)
    .WillRepeatedly(SetArgumentPointee<1>(processes));
  EXPECT_CALL(platform_, Unmount(EndsWith("/1"), true, _))
    .Times(2)
    .WillRepeatedly(Return(true));
  EXPECT_TRUE(service_.CleanUpStaleMounts(false));
}

}  // namespace cryptohome
