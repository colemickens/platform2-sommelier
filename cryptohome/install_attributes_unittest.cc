// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for InstallAttributes.

#include "cryptohome/install_attributes.h"

#include <string>
#include <vector>

#include <algorithm>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <brillo/secure_blob.h>
#include <gtest/gtest.h>

#include "cryptohome/lockbox.h"
#include "cryptohome/mock_lockbox.h"
#include "cryptohome/mock_platform.h"
#include "cryptohome/mock_tpm.h"
#include "cryptohome/mock_tpm_init.h"

namespace cryptohome {
using ::testing::DoAll;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SetArgPointee;
using ::testing::_;
using base::FilePath;

// Provides a test fixture for ensuring Lockbox-flows work as expected.
//
// Multiple helpers are included to ensure tests are starting from the same
// baseline for difference scenarios, such as first boot or all-other-normal
// boots.
class InstallAttributesTest : public ::testing::Test {
 public:
  InstallAttributesTest() : install_attrs_(NULL) { }
  virtual ~InstallAttributesTest() { }

  virtual void SetUp() {
    install_attrs_.set_lockbox(&lockbox_);
    install_attrs_.set_platform(&platform_);
    // No pre-existing data and no TPM auth.
    EXPECT_CALL(lockbox_, set_tpm(&tpm_))
      .Times(1);
    EXPECT_CALL(tpm_, IsEnabled())
      .WillOnce(Return(true));
    install_attrs_.SetTpm(&tpm_);
    Mock::VerifyAndClearExpectations(&lockbox_);
    Mock::VerifyAndClearExpectations(&tpm_);
  }

  virtual void TearDown() { }

  void GetAndCheck(InstallAttributes *install_attrs) {
    if (!install_attrs)
      install_attrs = &install_attrs_;
    brillo::Blob data;
    EXPECT_TRUE(install_attrs->Get(kTestName, &data));
    std::string data_str(reinterpret_cast<const char*>(data.data()),
                         data.size());
    EXPECT_STREQ(data_str.c_str(), kTestData);
  }

  // Tests a normal OOBE and stashes the data in the ptr.
  void DoOobe(InstallAttributes *install_attrs,
              brillo::Blob *serialized_data) {
    if (install_attrs->is_secure()) {
      EXPECT_CALL(lockbox_, Destroy(_))
        .WillOnce(Return(true));
    }
    EXPECT_TRUE(install_attrs->PrepareSystem());

    // Assume authorization and working tpm.
    EXPECT_CALL(lockbox_, tpm())
      .WillRepeatedly(Return(&tpm_));
    if (install_attrs->is_secure()) {
      EXPECT_CALL(lockbox_, Create(_))
        .WillOnce(Return(true));
      ExpectRemovingOwnerDependency();
    }
    EXPECT_TRUE(install_attrs->Init(&tpm_init_));

    brillo::Blob data;
    data.assign(kTestData, kTestData + strlen(kTestData));
    EXPECT_TRUE(install_attrs->Set(kTestName, data));

    if (install_attrs->is_secure()) {
      EXPECT_CALL(lockbox_, Store(_, _))
        .Times(1)
        .WillOnce(Return(true));
    }
    EXPECT_CALL(platform_,
        WriteFileAtomicDurable(
          FilePath(InstallAttributes::kDefaultDataFile),
          _, _))
      .Times(1)
      .WillOnce(DoAll(SaveArg<1>(serialized_data), Return(true)));
    brillo::Blob cached_data;
    EXPECT_CALL(platform_,
        WriteFileAtomic(
          FilePath(InstallAttributes::kDefaultCacheFile), _, _))
      .Times(1)
      .WillOnce(DoAll(SaveArg<1>(&cached_data), Return(true)));

    EXPECT_TRUE(install_attrs->Finalize());
    EXPECT_NE(0, serialized_data->size());
    ASSERT_EQ(serialized_data->size(), cached_data.size());
    EXPECT_TRUE(std::equal(cached_data.begin(), cached_data.end(),
                           serialized_data->begin()));
    Mock::VerifyAndClearExpectations(&lockbox_);
    Mock::VerifyAndClearExpectations(&platform_);
  }

  // Generate the data we'll need to load from.
  void PopulateOobeData(brillo::Blob *data) {
    InstallAttributes some_attrs(NULL);
    some_attrs.set_lockbox(&lockbox_);
    some_attrs.set_platform(&platform_);
    DoOobe(&some_attrs, data);
    Mock::VerifyAndClearExpectations(&lockbox_);
    Mock::VerifyAndClearExpectations(&platform_);
  }

  void ExpectRemovingOwnerDependency() {
    EXPECT_CALL(tpm_init_, RemoveTpmOwnerDependency(
        TpmPersistentState::TpmOwnerDependency::kInstallAttributes))
      .Times(1);
  }

  void ExpectNotRemovingOwnerDependency() {
    EXPECT_CALL(tpm_init_, RemoveTpmOwnerDependency(_))
      .Times(0);
  }

  static const char* kTestName;
  static const char* kTestData;

  NiceMock<MockLockbox> lockbox_;
  brillo::Blob lockbox_data_;
  InstallAttributes install_attrs_;
  NiceMock<MockPlatform> platform_;
  NiceMock<MockTpm> tpm_;
  NiceMock<MockTpmInit> tpm_init_;

 private:
  DISALLOW_COPY_AND_ASSIGN(InstallAttributesTest);
};
const char* InstallAttributesTest::kTestName = "Shuffle";
const char* InstallAttributesTest::kTestData = "Duffle";

//
// The actual tests!
//

// Normal bootup
TEST_F(InstallAttributesTest, OobeWithTpm) {
  brillo::Blob serialized_data;
  DoOobe(&install_attrs_, &serialized_data);
}

TEST_F(InstallAttributesTest, OobeWithoutTpm) {
  EXPECT_CALL(lockbox_, set_tpm(NULL))
    .Times(1);
  install_attrs_.SetTpm(NULL);

  EXPECT_CALL(platform_, ReadFile(_, _))
    .Times(1)
    .WillOnce(Return(false));
  ExpectNotRemovingOwnerDependency();
  EXPECT_TRUE(install_attrs_.Init(&tpm_init_));
  EXPECT_TRUE(install_attrs_.is_first_install());
}

TEST_F(InstallAttributesTest, OobeWithTpmBadWrite) {
  EXPECT_CALL(lockbox_, Destroy(_))
    .WillOnce(Return(true));
  EXPECT_TRUE(install_attrs_.PrepareSystem());

  // Assume authorization and working tpm.
  EXPECT_CALL(lockbox_, tpm())
    .WillRepeatedly(Return(&tpm_));
  EXPECT_CALL(lockbox_, Create(_))
    .WillOnce(Return(true));
  ExpectRemovingOwnerDependency();
  EXPECT_TRUE(install_attrs_.Init(&tpm_init_));

  brillo::Blob data;
  data.assign(kTestData, kTestData + strlen(kTestData));
  EXPECT_TRUE(install_attrs_.Set(kTestName, data));

  brillo::Blob serialized_data;
  EXPECT_CALL(lockbox_, Store(_, _))
    .Times(1)
    .WillOnce(DoAll(SaveArg<0>(&serialized_data), Return(true)));
  EXPECT_CALL(platform_, WriteFileAtomicDurable(_, _, _))
    .Times(1)
    .WillOnce(Return(false));
  EXPECT_FALSE(install_attrs_.Finalize());
  EXPECT_TRUE(install_attrs_.IsReady());
  EXPECT_TRUE(install_attrs_.is_invalid());
  EXPECT_TRUE(install_attrs_.is_initialized());
}

TEST_F(InstallAttributesTest, NormalBootWithTpm) {
  brillo::Blob serialized_data;
  PopulateOobeData(&serialized_data);

  // Check the baseline.
  EXPECT_FALSE(install_attrs_.is_first_install());
  EXPECT_FALSE(install_attrs_.is_initialized());
  EXPECT_FALSE(install_attrs_.is_invalid());

  EXPECT_CALL(platform_, ReadFile(_, _))
    .Times(1)
    .WillOnce(DoAll(SetArgPointee<1>(serialized_data), Return(true)));
  EXPECT_CALL(lockbox_, Load(_))
    .Times(1)
    .WillOnce(Return(true));
  EXPECT_CALL(lockbox_, Verify(_, _))
    .Times(1)
    .WillOnce(Return(true));
  ExpectRemovingOwnerDependency();
  EXPECT_TRUE(install_attrs_.Init(&tpm_init_));
  EXPECT_FALSE(install_attrs_.is_first_install());
  EXPECT_FALSE(install_attrs_.is_invalid());
  EXPECT_TRUE(install_attrs_.is_initialized());

  // Make sure the data was parsed correctly.
  EXPECT_EQ(1, install_attrs_.Count());
  GetAndCheck(NULL);
}

TEST_F(InstallAttributesTest, NormalBootWithoutTpm) {
  brillo::Blob serialized_data;
  PopulateOobeData(&serialized_data);

  EXPECT_CALL(lockbox_, set_tpm(NULL))
    .Times(1);
  install_attrs_.SetTpm(NULL);

  // Check the baseline.
  EXPECT_FALSE(install_attrs_.is_first_install());
  EXPECT_FALSE(install_attrs_.is_initialized());
  EXPECT_FALSE(install_attrs_.is_invalid());

  EXPECT_CALL(platform_, ReadFile(_, _))
    .Times(1)
    .WillOnce(DoAll(SetArgPointee<1>(serialized_data), Return(true)));

  ExpectRemovingOwnerDependency();
  EXPECT_TRUE(install_attrs_.Init(&tpm_init_));
  EXPECT_FALSE(install_attrs_.is_first_install());
  EXPECT_FALSE(install_attrs_.is_invalid());
  EXPECT_TRUE(install_attrs_.is_initialized());

  // Make sure the data was parsed correctly.
  EXPECT_EQ(1, install_attrs_.Count());
  GetAndCheck(NULL);
}

// Represents that the OOBE process was interrupted by
// a reboot or crash prior to Finalize() being called, but
// after the Lockbox was Created.
// Since InstallAttributes Set/Finalize is not atomic, there
// is always the risk of data loss due to failure of the device.
// It will fail-safe however (by failing empty).
TEST_F(InstallAttributesTest, NormalBootUnlocked) {
  // Normally, it should be impossible to populate the filesystem
  // with any data.  We put this here to show anything that may be
  // read in is ignored.
  brillo::Blob serialized_data;
  PopulateOobeData(&serialized_data);
  // Check the baseline.
  EXPECT_FALSE(install_attrs_.is_first_install());
  EXPECT_FALSE(install_attrs_.is_initialized());
  EXPECT_FALSE(install_attrs_.is_invalid());
  EXPECT_TRUE(install_attrs_.is_secure());

  LockboxError error = LockboxError::kNoNvramData;
  EXPECT_CALL(lockbox_, Load(_))
    .Times(1)
    .WillOnce(DoAll(SetArgPointee<0>(error), Return(false)));
  ExpectRemovingOwnerDependency();
  EXPECT_TRUE(install_attrs_.Init(&tpm_init_));
  EXPECT_TRUE(install_attrs_.is_first_install());
  EXPECT_FALSE(install_attrs_.is_invalid());
  EXPECT_TRUE(install_attrs_.is_initialized());

  // Should be empty.
  EXPECT_EQ(0, install_attrs_.Count());
}

// Represents that the OOBE process was interrupted by
// a reboot or crash prior to Finalize() being called, and
// before the Lockbox was Created.
TEST_F(InstallAttributesTest, NormalBootNoSpace) {
  // Normally, it should be impossible to populate the filesystem
  // with any data.  We put this here to show anything that may be
  // read in is ignored.
  brillo::Blob serialized_data;
  PopulateOobeData(&serialized_data);
  // Check the baseline.
  EXPECT_FALSE(install_attrs_.is_first_install());
  EXPECT_FALSE(install_attrs_.is_initialized());
  EXPECT_FALSE(install_attrs_.is_invalid());
  EXPECT_TRUE(install_attrs_.is_secure());

  LockboxError error = LockboxError::kNoNvramSpace;
  EXPECT_CALL(lockbox_, Load(_))
    .Times(1)
    .WillOnce(DoAll(SetArgPointee<0>(error), Return(false)));
  EXPECT_CALL(lockbox_, Create(_))
    .Times(1)
    .WillOnce(Return(true));
  ExpectRemovingOwnerDependency();
  EXPECT_TRUE(install_attrs_.Init(&tpm_init_));
  EXPECT_TRUE(install_attrs_.is_first_install());
  EXPECT_FALSE(install_attrs_.is_invalid());
  EXPECT_TRUE(install_attrs_.is_initialized());

  // Should be empty.
  EXPECT_EQ(0, install_attrs_.Count());
}

TEST_F(InstallAttributesTest, NormalBootLoadError) {
  // Check the baseline.
  EXPECT_FALSE(install_attrs_.is_first_install());
  EXPECT_FALSE(install_attrs_.is_initialized());
  EXPECT_FALSE(install_attrs_.is_invalid());

  LockboxError error = LockboxError::kTpmError;
  EXPECT_CALL(lockbox_, Load(_))
    .Times(1)
    .WillOnce(DoAll(SetArgPointee<0>(error), Return(false)));
  ExpectNotRemovingOwnerDependency();
  EXPECT_FALSE(install_attrs_.Init(&tpm_init_));
  EXPECT_FALSE(install_attrs_.is_first_install());
  EXPECT_TRUE(install_attrs_.is_invalid());
  EXPECT_FALSE(install_attrs_.is_initialized());

  // Should be empty.
  EXPECT_EQ(0, install_attrs_.Count());
}

TEST_F(InstallAttributesTest, NormalBootReadFileError) {
  // Check the baseline.
  EXPECT_FALSE(install_attrs_.is_first_install());
  EXPECT_FALSE(install_attrs_.is_initialized());
  EXPECT_FALSE(install_attrs_.is_invalid());

  EXPECT_CALL(lockbox_, Load(_))
    .Times(1)
    .WillOnce(Return(true));
  EXPECT_CALL(platform_, ReadFile(_, _))
    .Times(1)
    .WillOnce(Return(false));
  ExpectNotRemovingOwnerDependency();
  EXPECT_FALSE(install_attrs_.Init(&tpm_init_));
  EXPECT_FALSE(install_attrs_.is_first_install());
  EXPECT_TRUE(install_attrs_.is_invalid());
  EXPECT_FALSE(install_attrs_.is_initialized());

  // Should be empty.
  EXPECT_EQ(0, install_attrs_.Count());
}

TEST_F(InstallAttributesTest, NormalBootVerifyError) {
  // Check the baseline.
  EXPECT_FALSE(install_attrs_.is_first_install());
  EXPECT_FALSE(install_attrs_.is_initialized());
  EXPECT_FALSE(install_attrs_.is_invalid());

  LockboxError error = LockboxError::kHashMismatch;
  EXPECT_CALL(lockbox_, Load(_))
    .Times(1)
    .WillOnce(Return(true));

  EXPECT_CALL(platform_, ReadFile(_, _))
    .Times(1)
    .WillOnce(Return(true));

  EXPECT_CALL(lockbox_, Verify(_, _))
    .Times(1)
    .WillOnce(DoAll(SetArgPointee<1>(error), Return(false)));

  ExpectNotRemovingOwnerDependency();
  EXPECT_FALSE(install_attrs_.Init(&tpm_init_));
  EXPECT_FALSE(install_attrs_.is_first_install());
  EXPECT_TRUE(install_attrs_.is_invalid());
  EXPECT_FALSE(install_attrs_.is_initialized());

  // Should be empty.
  EXPECT_EQ(0, install_attrs_.Count());
}

TEST_F(InstallAttributesTest, LegacyBoot) {
  // Check the baseline.
  EXPECT_FALSE(install_attrs_.is_first_install());
  EXPECT_FALSE(install_attrs_.is_initialized());
  EXPECT_FALSE(install_attrs_.is_invalid());

  LockboxError error = LockboxError::kNoNvramSpace;
  EXPECT_CALL(lockbox_, Load(_))
    .Times(1)
    .WillOnce(DoAll(SetArgPointee<0>(error), Return(false)));
  LockboxError create_error = LockboxError::kInsufficientAuthorization;
  EXPECT_CALL(lockbox_, Create(_))
    .Times(1)
    .WillOnce(DoAll(SetArgPointee<0>(create_error), Return(false)));
  ExpectRemovingOwnerDependency();
  EXPECT_TRUE(install_attrs_.Init(&tpm_init_));
  EXPECT_FALSE(install_attrs_.is_first_install());
  EXPECT_FALSE(install_attrs_.is_invalid());
  EXPECT_TRUE(install_attrs_.is_initialized());

  // Should be empty.
  EXPECT_EQ(0, install_attrs_.Count());
}

// If the Lockbox Create fails for reasons other than bad password, it
// should still be treated as a LegacyBoot.
TEST_F(InstallAttributesTest, LegacyBootUnexpected) {
  // Check the baseline.
  EXPECT_FALSE(install_attrs_.is_first_install());
  EXPECT_FALSE(install_attrs_.is_initialized());
  EXPECT_FALSE(install_attrs_.is_invalid());

  LockboxError error = LockboxError::kNoNvramSpace;
  EXPECT_CALL(lockbox_, Load(_))
    .Times(1)
    .WillOnce(DoAll(SetArgPointee<0>(error), Return(false)));
  LockboxError create_error = LockboxError::kTpmError;
  EXPECT_CALL(lockbox_, Create(_))
    .Times(1)
    .WillOnce(DoAll(SetArgPointee<0>(create_error), Return(false)));
  ExpectRemovingOwnerDependency();
  EXPECT_TRUE(install_attrs_.Init(&tpm_init_));
  EXPECT_FALSE(install_attrs_.is_first_install());
  EXPECT_FALSE(install_attrs_.is_invalid());
  EXPECT_TRUE(install_attrs_.is_initialized());

  // Should be empty.
  EXPECT_EQ(0, install_attrs_.Count());
}

}  // namespace cryptohome
