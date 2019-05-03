// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/userdataauth.h"

#include "cryptohome/mock_crypto.h"
#include "cryptohome/mock_homedirs.h"
#include "cryptohome/mock_mount.h"
#include "cryptohome/mock_platform.h"
#include "cryptohome/mock_tpm.h"
#include "cryptohome/mock_tpm_init.h"

using brillo::SecureBlob;

using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
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
    userdataauth_.set_disable_threading(true);
    homedirs_.set_crypto(&crypto_);
    homedirs_.set_platform(&platform_);
    ON_CALL(homedirs_, Init(_, _, _)).WillByDefault(Return(true));
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

}  // namespace cryptohome
