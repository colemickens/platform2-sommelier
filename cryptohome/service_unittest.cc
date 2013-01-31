// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
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
#include "mock_platform.h"
#include "mock_tpm.h"
#include "username_passkey.h"

using base::PlatformThread;
using chromeos::SecureBlob;

namespace cryptohome {
using ::testing::Return;
using ::testing::_;
using ::testing::NiceMock;

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
  MockMount mount;
  MockHomeDirs homedirs;
  EXPECT_CALL(homedirs, Init())
      .WillOnce(Return(true));
  EXPECT_CALL(mount, Init())
      .WillOnce(Return(true));
  EXPECT_CALL(mount, AreSameUser(_))
      .WillOnce(Return(false));
  EXPECT_CALL(homedirs, AreCredentialsValid(_))
      .WillOnce(Return(true));

  Service service;
  service.set_homedirs(&homedirs);
  service.set_mount_for_user("", &mount);
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
  Mount mount;
  NiceMock<MockTpm> tpm;
  NiceMock<MockPlatform> platform;

  test_helper_.InjectSystemSalt(&platform, kSaltFile);
  test_helper_.InitTestData(kImageDir, kDefaultUsers, kDefaultUserCount);
  TestUser* user = &test_helper_.users[7];
  user->InjectKeyset(&platform);

  mount.crypto()->set_tpm(&tpm);
  mount.crypto()->set_platform(&platform);
  mount.homedirs()->set_platform(&platform);
  mount.homedirs()->crypto()->set_platform(&platform);
  mount.set_platform(&platform);
  mount.set_shadow_root(kImageDir);
  mount.set_skel_source(kSkelDir);
  mount.set_use_tpm(false);
  mount.set_policy_provider(new policy::PolicyProvider(
      new NiceMock<policy::MockDevicePolicy>));

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
  service.set_mount_for_user("", &mount);
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
  NiceMock<MockMount> mount;
  MockHomeDirs homedirs;
  Service service;
  service.set_mount_for_user("", &mount);
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
  EXPECT_CALL(mount, UpdateCurrentUserActivityTimestamp(0))
      .Times(::testing::AtLeast(3));
  service.set_auto_cleanup_period(2);  // 2ms = 500HZ
  service.set_update_user_activity_period(2);  // 2 x 5ms = 25HZ
  service.Initialize();
  PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(100));
}

TEST(Standalone, CheckAutoCleanupCallbackFirst) {
  // Checks that AutoCleanupCallback() is called first right after init.
  NiceMock<MockMount> mount;
  MockHomeDirs homedirs;
  Service service;
  service.set_mount_for_user("", &mount);
  service.set_homedirs(&homedirs);
  NiceMock<MockInstallAttributes> attrs;
  service.set_install_attrs(&attrs);
  service.set_initialize_tpm(false);

  // Service will schedule first claenup right after its init.
  EXPECT_CALL(homedirs, Init())
      .WillOnce(Return(true));
  EXPECT_CALL(homedirs, FreeDiskSpace())
      .Times(1);
  service.set_auto_cleanup_period(1000);  // 1s - long enough
  service.Initialize();
  // short delay to see the first invocation
  PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(10));
}

}  // namespace cryptohome
