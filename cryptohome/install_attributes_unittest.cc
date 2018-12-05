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

using base::FilePath;
using ::testing::_;
using ::testing::DoAll;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SetArgPointee;

namespace cryptohome {

namespace {
static constexpr char kTestName[] = "Shuffle";
static constexpr char kTestData[] = "Duffle";
}  // namespace

// Provides a test fixture for ensuring Lockbox-flows work as expected.
//
// Multiple helpers are included to ensure tests are starting from the same
// baseline for difference scenarios, such as first boot or all-other-normal
// boots.
class InstallAttributesTest : public ::testing::Test {
 public:
  InstallAttributesTest() : install_attrs_(nullptr) {}
  ~InstallAttributesTest() override = default;

  void SetUp() override {
    ON_CALL(tpm_init_, IsTpmReady()).WillByDefault(Return(true));
    ON_CALL(tpm_init_, IsTpmEnabled()).WillByDefault(Return(true));
    ON_CALL(tpm_init_, IsTpmOwned()).WillByDefault(Return(true));

    install_attrs_.set_lockbox(&lockbox_);
    install_attrs_.set_platform(&platform_);
    // No pre-existing data and no TPM auth.
    EXPECT_CALL(lockbox_, set_tpm(&tpm_)).Times(1);
    EXPECT_CALL(tpm_, IsEnabled()).WillOnce(Return(true));
    install_attrs_.SetTpm(&tpm_);
    Mock::VerifyAndClearExpectations(&lockbox_);
    Mock::VerifyAndClearExpectations(&tpm_);
  }

  void GetAndCheck() {
    EXPECT_EQ(1, install_attrs_.Count());
    brillo::Blob data;
    EXPECT_TRUE(install_attrs_.Get(kTestName, &data));
    std::string data_str(reinterpret_cast<const char*>(data.data()),
                         data.size());
    EXPECT_STREQ(data_str.c_str(), kTestData);
  }

  // Generate the data we'll need to load from.
  brillo::Blob GenerateTestDataFileContents() {
    brillo::Blob data;
    SerializedInstallAttributes proto;
    proto.set_version(proto.version());
    SerializedInstallAttributes::Attribute* attr = proto.add_attributes();
    attr->set_name(kTestName);
    attr->set_value(std::string(reinterpret_cast<const char*>(kTestData),
                                strlen(kTestData)));
    data.resize(proto.ByteSize());
    CHECK(proto.SerializeWithCachedSizesToArray(data.data()));
    return data;
  }

  void ExpectRemovingOwnerDependency() {
    EXPECT_CALL(tpm_init_,
                RemoveTpmOwnerDependency(
                    TpmPersistentState::TpmOwnerDependency::kInstallAttributes))
        .Times(1);
  }

  void ExpectNotRemovingOwnerDependency() {
    EXPECT_CALL(tpm_init_, RemoveTpmOwnerDependency(_)).Times(0);
  }

  NiceMock<MockLockbox> lockbox_;
  brillo::Blob lockbox_data_;
  InstallAttributes install_attrs_;
  NiceMock<MockPlatform> platform_;
  NiceMock<MockTpm> tpm_;
  NiceMock<MockTpmInit> tpm_init_;

 private:
  DISALLOW_COPY_AND_ASSIGN(InstallAttributesTest);
};

TEST_F(InstallAttributesTest, OobeWithTpm) {
  // The first Init() call finds no data file and an unowned TPM.
  EXPECT_CALL(platform_,
              ReadFile(FilePath(InstallAttributes::kDefaultCacheFile), _))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(tpm_init_, IsTpmReady()).WillRepeatedly(Return(false));
  EXPECT_CALL(tpm_init_, IsTpmOwned()).WillRepeatedly(Return(false));
  EXPECT_FALSE(install_attrs_.Init(&tpm_init_));
  Mock::VerifyAndClearExpectations(&tpm_init_);
  Mock::VerifyAndClearExpectations(&platform_);

  EXPECT_FALSE(install_attrs_.IsReady());

  // After taking ownership, TPM is ready and Init creates the lockbox.
  EXPECT_CALL(platform_,
              ReadFile(FilePath(InstallAttributes::kDefaultCacheFile), _))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(lockbox_, Create(_)).WillOnce(Return(true));
  ExpectRemovingOwnerDependency();
  EXPECT_TRUE(install_attrs_.Init(&tpm_init_));
  Mock::VerifyAndClearExpectations(&lockbox_);
  Mock::VerifyAndClearExpectations(&platform_);

  EXPECT_TRUE(install_attrs_.is_initialized());
  EXPECT_TRUE(install_attrs_.is_first_install());
  EXPECT_FALSE(install_attrs_.is_invalid());

  // Set the test attribute.
  brillo::Blob data(kTestData, kTestData + sizeof(kTestData));
  data.assign(kTestData, kTestData + strlen(kTestData));
  EXPECT_TRUE(install_attrs_.Set(kTestName, data));

  // Finalize.
  EXPECT_CALL(lockbox_, Store(_, _)).WillOnce(Return(true));
  brillo::Blob serialized_data;
  EXPECT_CALL(platform_,
              WriteFileAtomicDurable(
                  FilePath(InstallAttributes::kDefaultDataFile), _, _))
      .WillOnce(DoAll(SaveArg<1>(&serialized_data), Return(true)));
  brillo::Blob cached_data;
  EXPECT_CALL(
      platform_,
      WriteFileAtomic(FilePath(InstallAttributes::kDefaultCacheFile), _, _))
      .WillOnce(DoAll(SaveArg<1>(&cached_data), Return(true)));

  EXPECT_TRUE(install_attrs_.Finalize());
  Mock::VerifyAndClearExpectations(&lockbox_);
  Mock::VerifyAndClearExpectations(&platform_);

  EXPECT_TRUE(install_attrs_.is_initialized());
  EXPECT_FALSE(install_attrs_.is_first_install());
  EXPECT_FALSE(install_attrs_.is_invalid());

  brillo::Blob expected_data = GenerateTestDataFileContents();
  EXPECT_EQ(expected_data, serialized_data);
  EXPECT_EQ(expected_data, cached_data);
}

TEST_F(InstallAttributesTest, OobeWithoutTpm) {
  EXPECT_CALL(lockbox_, set_tpm(nullptr)).Times(1);
  install_attrs_.SetTpm(nullptr);
  Mock::VerifyAndClearExpectations(&lockbox_);

  EXPECT_CALL(platform_,
              ReadFile(FilePath(InstallAttributes::kDefaultCacheFile), _))
      .WillOnce(Return(false));
  ExpectNotRemovingOwnerDependency();

  EXPECT_TRUE(install_attrs_.Init(&tpm_init_));

  EXPECT_TRUE(install_attrs_.is_first_install());
}

TEST_F(InstallAttributesTest, OobeWithTpmBadWrite) {
  // Assume authorization and working tpm.
  EXPECT_CALL(lockbox_, tpm()).WillRepeatedly(Return(&tpm_));
  EXPECT_CALL(lockbox_, Create(_)).WillOnce(Return(true));
  ExpectRemovingOwnerDependency();

  EXPECT_TRUE(install_attrs_.Init(&tpm_init_));
  Mock::VerifyAndClearExpectations(&lockbox_);

  brillo::Blob data;
  data.assign(kTestData, kTestData + strlen(kTestData));
  EXPECT_TRUE(install_attrs_.Set(kTestName, data));

  EXPECT_CALL(lockbox_, Store(_, _)).WillOnce(Return(true));
  EXPECT_CALL(platform_, WriteFileAtomicDurable(_, _, _))
      .WillOnce(Return(false));

  EXPECT_FALSE(install_attrs_.Finalize());

  EXPECT_TRUE(install_attrs_.IsReady());
  EXPECT_TRUE(install_attrs_.is_invalid());
  EXPECT_TRUE(install_attrs_.is_initialized());
}

TEST_F(InstallAttributesTest, NormalBootWithTpm) {
  // Check the baseline.
  EXPECT_FALSE(install_attrs_.is_first_install());
  EXPECT_FALSE(install_attrs_.is_initialized());
  EXPECT_FALSE(install_attrs_.is_invalid());

  brillo::Blob serialized_data = GenerateTestDataFileContents();
  EXPECT_CALL(platform_,
              ReadFile(FilePath(InstallAttributes::kDefaultCacheFile), _))
      .WillOnce(DoAll(SetArgPointee<1>(serialized_data), Return(true)));
  ExpectRemovingOwnerDependency();

  EXPECT_TRUE(install_attrs_.Init(&tpm_init_));

  EXPECT_FALSE(install_attrs_.is_first_install());
  EXPECT_FALSE(install_attrs_.is_invalid());
  EXPECT_TRUE(install_attrs_.is_initialized());

  // Make sure the data was parsed correctly.
  GetAndCheck();
}

TEST_F(InstallAttributesTest, NormalBootWithoutTpm) {
  brillo::Blob serialized_data = GenerateTestDataFileContents();

  EXPECT_CALL(lockbox_, set_tpm(nullptr)).Times(1);
  install_attrs_.SetTpm(nullptr);

  // Check the baseline.
  EXPECT_FALSE(install_attrs_.is_first_install());
  EXPECT_FALSE(install_attrs_.is_initialized());
  EXPECT_FALSE(install_attrs_.is_invalid());

  EXPECT_CALL(platform_,
              ReadFile(FilePath(InstallAttributes::kDefaultCacheFile), _))
      .WillOnce(DoAll(SetArgPointee<1>(serialized_data), Return(true)));
  ExpectRemovingOwnerDependency();

  EXPECT_TRUE(install_attrs_.Init(&tpm_init_));

  EXPECT_FALSE(install_attrs_.is_first_install());
  EXPECT_FALSE(install_attrs_.is_invalid());
  EXPECT_TRUE(install_attrs_.is_initialized());

  // Make sure the data was parsed correctly.
  GetAndCheck();
}

// Represents that the OOBE process was interrupted by a reboot or crash prior
// to Finalize() being called.
TEST_F(InstallAttributesTest, NormalBootUnlocked) {
  // Check the baseline.
  EXPECT_FALSE(install_attrs_.is_first_install());
  EXPECT_FALSE(install_attrs_.is_initialized());
  EXPECT_FALSE(install_attrs_.is_invalid());
  EXPECT_TRUE(install_attrs_.is_secure());

  EXPECT_CALL(platform_,
              ReadFile(FilePath(InstallAttributes::kDefaultCacheFile), _))
      .WillOnce(Return(false));
  EXPECT_CALL(lockbox_, Create(_))
      .WillOnce(
          DoAll(SetArgPointee<0>(LockboxError::kNoNvramData), Return(true)));
  ExpectRemovingOwnerDependency();

  EXPECT_TRUE(install_attrs_.Init(&tpm_init_));

  EXPECT_TRUE(install_attrs_.is_first_install());
  EXPECT_FALSE(install_attrs_.is_invalid());
  EXPECT_TRUE(install_attrs_.is_initialized());

  // Should be empty.
  EXPECT_EQ(0, install_attrs_.Count());
}

TEST_F(InstallAttributesTest, NormalBootReadFileError) {
  // Check the baseline.
  EXPECT_FALSE(install_attrs_.is_first_install());
  EXPECT_FALSE(install_attrs_.is_initialized());
  EXPECT_FALSE(install_attrs_.is_invalid());

  EXPECT_CALL(platform_,
              ReadFile(FilePath(InstallAttributes::kDefaultCacheFile), _))
      .WillOnce(Return(false));
  EXPECT_CALL(lockbox_, Create(_))
      .WillOnce(
          DoAll(SetArgPointee<0>(LockboxError::kInsufficientAuthorization),
                Return(false)));
  EXPECT_CALL(lockbox_, Load(_)).WillOnce(Return(true));
  ExpectNotRemovingOwnerDependency();
  EXPECT_CALL(platform_, DeleteFile(_, _)).Times(0);

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

  EXPECT_CALL(platform_,
              ReadFile(FilePath(InstallAttributes::kDefaultCacheFile), _))
      .WillOnce(Return(false));
  EXPECT_CALL(lockbox_, Create(_))
      .WillOnce(
          DoAll(SetArgPointee<0>(LockboxError::kInsufficientAuthorization),
                Return(false)));
  EXPECT_CALL(lockbox_, Load(_))
      .WillOnce(
          DoAll(SetArgPointee<0>(LockboxError::kNoNvramSpace), Return(false)));
  ExpectRemovingOwnerDependency();

  EXPECT_TRUE(install_attrs_.Init(&tpm_init_));

  EXPECT_FALSE(install_attrs_.is_first_install());
  EXPECT_FALSE(install_attrs_.is_invalid());
  EXPECT_TRUE(install_attrs_.is_initialized());

  // Should be empty.
  EXPECT_EQ(0, install_attrs_.Count());
}

// If the Lockbox Reset fails for reasons other than bad password, it should
// still be treated as if locked without any attributes set.
TEST_F(InstallAttributesTest, LegacyBootUnexpected) {
  // Check the baseline.
  EXPECT_FALSE(install_attrs_.is_first_install());
  EXPECT_FALSE(install_attrs_.is_initialized());
  EXPECT_FALSE(install_attrs_.is_invalid());

  EXPECT_CALL(platform_,
              ReadFile(FilePath(InstallAttributes::kDefaultCacheFile), _))
      .WillOnce(Return(false));
  EXPECT_CALL(lockbox_, Create(_))
      .WillOnce(
          DoAll(SetArgPointee<0>(LockboxError::kTpmError), Return(false)));
  ExpectRemovingOwnerDependency();

  EXPECT_TRUE(install_attrs_.Init(&tpm_init_));

  EXPECT_FALSE(install_attrs_.is_first_install());
  EXPECT_FALSE(install_attrs_.is_invalid());
  EXPECT_TRUE(install_attrs_.is_initialized());

  // Should be empty.
  EXPECT_EQ(0, install_attrs_.Count());
}

// If initializing with an unowned TPM, the old data file should be deleted to
// make sure that we don't accidentally pick it up as valid after taking
// ownership.
TEST_F(InstallAttributesTest, ClearPreviousDataFile) {
  // Check the baseline.
  EXPECT_FALSE(install_attrs_.is_first_install());
  EXPECT_FALSE(install_attrs_.is_initialized());
  EXPECT_FALSE(install_attrs_.is_invalid());

  EXPECT_CALL(tpm_init_, IsTpmReady()).WillRepeatedly(Return(false));
  EXPECT_CALL(tpm_init_, IsTpmOwned()).WillRepeatedly(Return(false));

  // The cache file isn't present because lockbox-cache won't receive a dump of
  // the lockbox space if the TPM isn't owned.
  EXPECT_CALL(platform_,
              ReadFile(FilePath(InstallAttributes::kDefaultCacheFile), _))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(platform_,
              FileExists(FilePath(InstallAttributes::kDefaultDataFile)))
      .WillOnce(Return(true));
  EXPECT_CALL(platform_,
              DeleteFile(FilePath(InstallAttributes::kDefaultDataFile), _))
      .WillOnce(Return(true));

  EXPECT_FALSE(install_attrs_.Init(&tpm_init_));

  EXPECT_FALSE(install_attrs_.is_first_install());
  EXPECT_FALSE(install_attrs_.is_invalid());
  EXPECT_FALSE(install_attrs_.is_initialized());

  // Should be empty.
  EXPECT_EQ(0, install_attrs_.Count());
}

// Check that if the TPM is out for lunch and inoperable in this boot cycle, we
// do keep around the data file as to not irrevocably invalidate install
// attributes should the TPM start functioning again after reboot.
TEST_F(InstallAttributesTest, KeepDataFileOnTpmFailure) {
  // Check the baseline.
  EXPECT_FALSE(install_attrs_.is_first_install());
  EXPECT_FALSE(install_attrs_.is_initialized());
  EXPECT_FALSE(install_attrs_.is_invalid());

  EXPECT_CALL(tpm_init_, IsTpmReady()).WillRepeatedly(Return(false));
  EXPECT_CALL(tpm_init_, IsTpmEnabled()).WillRepeatedly(Return(false));
  EXPECT_CALL(tpm_init_, IsTpmOwned()).WillRepeatedly(Return(false));

  // The cache file isn't present because lockbox-cache won't receive a dump of
  // the lockbox space if the TPM isn't owned.
  EXPECT_CALL(platform_,
              ReadFile(FilePath(InstallAttributes::kDefaultCacheFile), _))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(platform_,
              FileExists(FilePath(InstallAttributes::kDefaultDataFile)))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(platform_,
              DeleteFile(FilePath(InstallAttributes::kDefaultDataFile), _))
      .Times(0);

  EXPECT_FALSE(install_attrs_.Init(&tpm_init_));

  EXPECT_FALSE(install_attrs_.is_first_install());
  EXPECT_TRUE(install_attrs_.is_invalid());
  EXPECT_FALSE(install_attrs_.is_initialized());

  // Should be empty.
  EXPECT_EQ(0, install_attrs_.Count());
}

}  // namespace cryptohome
