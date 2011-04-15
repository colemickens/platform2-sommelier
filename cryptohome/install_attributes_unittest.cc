// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for InstallAttributes.

#include "install_attributes.h"

#include <base/file_util.h>
#include <base/logging.h>
#include <chromeos/utility.h>
#include <gtest/gtest.h>
#include <vector>

#include "lockbox.h"
#include "mock_lockbox.h"
#include "mock_platform.h"
#include "mock_tpm.h"

namespace cryptohome {
using std::string;
using ::testing::_;
using ::testing::DoAll;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SetArgumentPointee;

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
    chromeos::Blob data;
    EXPECT_TRUE(install_attrs->Get(kTestName, &data));
    std::string data_str(reinterpret_cast<const char*>(&data[0]),
                         data.size());
    EXPECT_STREQ(data_str.c_str(), kTestData);
  }

  // Tests a normal OOBE and stashes the data in the ptr.
  void DoOobe(InstallAttributes *install_attrs,
              chromeos::Blob *serialized_data) {
    if (install_attrs->is_secure()) {
      EXPECT_CALL(lockbox_, Destroy(_))
        .WillOnce(Return(true));
    }
    EXPECT_TRUE(install_attrs->PrepareSystem());

    // Assume authorization and working tpm.
    if (install_attrs->is_secure()) {
      EXPECT_CALL(lockbox_, Create(_))
        .WillOnce(Return(true));
    }
    EXPECT_TRUE(install_attrs->Init());

    chromeos::Blob data;
    data.assign(kTestData, kTestData + strlen(kTestData));
    EXPECT_TRUE(install_attrs->Set(kTestName, data));

    if (install_attrs->is_secure()) {
      EXPECT_CALL(lockbox_, Store(_, _))
        .Times(1)
        .WillOnce(Return(true));
    }
    EXPECT_CALL(platform_, WriteFile(_, _))
      .Times(1)
      .WillOnce(DoAll(SaveArg<1>(serialized_data), Return(true)));
    EXPECT_TRUE(install_attrs->Finalize());
    EXPECT_NE(0, serialized_data->size());
    Mock::VerifyAndClearExpectations(&lockbox_);
    Mock::VerifyAndClearExpectations(&platform_);
  }

  // Generate the data we'll need to load from.
  void PopulateOobeData(chromeos::Blob *data) {
    InstallAttributes some_attrs(NULL);
    some_attrs.set_lockbox(&lockbox_);
    some_attrs.set_platform(&platform_);
    DoOobe(&some_attrs, data);
    Mock::VerifyAndClearExpectations(&lockbox_);
    Mock::VerifyAndClearExpectations(&platform_);
  }

  static const char* kTestName;
  static const char* kTestData;

  NiceMock<MockLockbox> lockbox_;
  chromeos::Blob lockbox_data_;
  InstallAttributes install_attrs_;
  NiceMock<MockPlatform> platform_;
  NiceMock<MockTpm> tpm_;
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
  chromeos::Blob serialized_data;
  DoOobe(&install_attrs_, &serialized_data);
}

TEST_F(InstallAttributesTest, OobeWithoutTpm) {
  EXPECT_CALL(lockbox_, set_tpm(NULL))
    .Times(1);
  install_attrs_.SetTpm(NULL);

  EXPECT_CALL(platform_, ReadFile(_, _))
   .Times(1)
   .WillOnce(Return(false));

  EXPECT_TRUE(install_attrs_.Init());
  EXPECT_TRUE(install_attrs_.is_first_install());
}

TEST_F(InstallAttributesTest, OobeWithTpmBadWrite) {
  EXPECT_CALL(lockbox_, Destroy(_))
    .WillOnce(Return(true));
  EXPECT_TRUE(install_attrs_.PrepareSystem());

  // Assume authorization and working tpm.
  EXPECT_CALL(lockbox_, Create(_))
    .WillOnce(Return(true));
  EXPECT_TRUE(install_attrs_.Init());

  chromeos::Blob data;
  data.assign(kTestData, kTestData + strlen(kTestData));
  EXPECT_TRUE(install_attrs_.Set(kTestName, data));

  chromeos::Blob serialized_data;
  EXPECT_CALL(lockbox_, Store(_, _))
    .Times(1)
    .WillOnce(DoAll(SaveArg<0>(&serialized_data), Return(true)));
  EXPECT_CALL(platform_, WriteFile(_, _))
    .Times(1)
    .WillOnce(Return(false));
  EXPECT_FALSE(install_attrs_.Finalize());
  EXPECT_TRUE(install_attrs_.IsReady());
  EXPECT_TRUE(install_attrs_.is_invalid());
  EXPECT_TRUE(install_attrs_.is_initialized());
}

TEST_F(InstallAttributesTest, NormalBootWithTpm) {
  chromeos::Blob serialized_data;
  PopulateOobeData(&serialized_data);

  // Check the baseline.
  EXPECT_EQ(false, install_attrs_.is_first_install());
  EXPECT_EQ(false, install_attrs_.is_initialized());
  EXPECT_EQ(false, install_attrs_.is_invalid());

  EXPECT_CALL(platform_, ReadFile(_, _))
   .Times(1)
   .WillOnce(DoAll(SetArgumentPointee<1>(serialized_data), Return(true)));
  EXPECT_CALL(lockbox_, Load(_))
   .Times(1)
   .WillOnce(Return(true));
  EXPECT_CALL(lockbox_, Verify(_, _))
   .Times(1)
   .WillOnce(Return(true));
  EXPECT_TRUE(install_attrs_.Init());
  EXPECT_FALSE(install_attrs_.is_first_install());
  EXPECT_FALSE(install_attrs_.is_invalid());
  EXPECT_TRUE(install_attrs_.is_initialized());

  // Make sure the data was parsed correctly.
  EXPECT_EQ(1, install_attrs_.Count());
  GetAndCheck(NULL);
}

TEST_F(InstallAttributesTest, NormalBootWithoutTpm) {
  chromeos::Blob serialized_data;
  PopulateOobeData(&serialized_data);

  EXPECT_CALL(lockbox_, set_tpm(NULL))
    .Times(1);
  install_attrs_.SetTpm(NULL);

  // Check the baseline.
  EXPECT_EQ(false, install_attrs_.is_first_install());
  EXPECT_EQ(false, install_attrs_.is_initialized());
  EXPECT_EQ(false, install_attrs_.is_invalid());

  EXPECT_CALL(platform_, ReadFile(_, _))
   .Times(1)
   .WillOnce(DoAll(SetArgumentPointee<1>(serialized_data), Return(true)));

  EXPECT_TRUE(install_attrs_.Init());
  EXPECT_FALSE(install_attrs_.is_first_install());
  EXPECT_FALSE(install_attrs_.is_invalid());
  EXPECT_TRUE(install_attrs_.is_initialized());

  // Make sure the data was parsed correctly.
  EXPECT_EQ(1, install_attrs_.Count());
  GetAndCheck(NULL);
}

// Represents that the OOBE process was interrupted by
// a reboot or crash prior to Finalize() being called.
// Since InstallAttributes Set/Finalize is not atomic, there
// is always the risk of data loss due to failure of the device.
// It will fail-safe however (by failing empty).
TEST_F(InstallAttributesTest, NormalBootUnlocked) {
  // Normally, it should be impossible to populate the filesystem
  // with any data.  We put this here to show anything that may be
  // read in is ignored.
  chromeos::Blob serialized_data;
  PopulateOobeData(&serialized_data);
  // Check the baseline.
  EXPECT_EQ(false, install_attrs_.is_first_install());
  EXPECT_EQ(false, install_attrs_.is_initialized());
  EXPECT_EQ(false, install_attrs_.is_invalid());
  EXPECT_EQ(true, install_attrs_.is_secure());

  Lockbox::ErrorId error_id = Lockbox::kErrorIdNoNvramData;
  EXPECT_CALL(lockbox_, Load(_))
   .Times(1)
   .WillOnce(DoAll(SetArgumentPointee<0>(error_id), Return(false)));
  EXPECT_TRUE(install_attrs_.Init());
  EXPECT_TRUE(install_attrs_.is_first_install());
  EXPECT_FALSE(install_attrs_.is_invalid());
  EXPECT_TRUE(install_attrs_.is_initialized());

  // Should be empty.
  EXPECT_EQ(0, install_attrs_.Count());
}

TEST_F(InstallAttributesTest, NormalBootLoadError) {
  // Check the baseline.
  EXPECT_EQ(false, install_attrs_.is_first_install());
  EXPECT_EQ(false, install_attrs_.is_initialized());
  EXPECT_EQ(false, install_attrs_.is_invalid());

  Lockbox::ErrorId error_id = Lockbox::kErrorIdTpmError;
  EXPECT_CALL(lockbox_, Load(_))
   .Times(1)
   .WillOnce(DoAll(SetArgumentPointee<0>(error_id), Return(false)));
  EXPECT_FALSE(install_attrs_.Init());
  EXPECT_FALSE(install_attrs_.is_first_install());
  EXPECT_TRUE(install_attrs_.is_invalid());
  EXPECT_FALSE(install_attrs_.is_initialized());

  // Should be empty.
  EXPECT_EQ(0, install_attrs_.Count());
}

TEST_F(InstallAttributesTest, NormalBootReadFileError) {
  // Check the baseline.
  EXPECT_EQ(false, install_attrs_.is_first_install());
  EXPECT_EQ(false, install_attrs_.is_initialized());
  EXPECT_EQ(false, install_attrs_.is_invalid());

  EXPECT_CALL(lockbox_, Load(_))
   .Times(1)
   .WillOnce(Return(true));
  EXPECT_CALL(platform_, ReadFile(_, _))
   .Times(1)
   .WillOnce(Return(false));
  EXPECT_FALSE(install_attrs_.Init());
  EXPECT_FALSE(install_attrs_.is_first_install());
  EXPECT_TRUE(install_attrs_.is_invalid());
  EXPECT_FALSE(install_attrs_.is_initialized());

  // Should be empty.
  EXPECT_EQ(0, install_attrs_.Count());
}

TEST_F(InstallAttributesTest, NormalBootVerifyError) {
  // Check the baseline.
  EXPECT_EQ(false, install_attrs_.is_first_install());
  EXPECT_EQ(false, install_attrs_.is_initialized());
  EXPECT_EQ(false, install_attrs_.is_invalid());

  Lockbox::ErrorId error_id = Lockbox::kErrorIdHashMismatch;
  EXPECT_CALL(lockbox_, Load(_))
   .Times(1)
   .WillOnce(Return(true));

  EXPECT_CALL(platform_, ReadFile(_, _))
   .Times(1)
   .WillOnce(Return(true));

  EXPECT_CALL(lockbox_, Verify(_, _))
   .Times(1)
   .WillOnce(DoAll(SetArgumentPointee<1>(error_id), Return(false)));

  EXPECT_FALSE(install_attrs_.Init());
  EXPECT_FALSE(install_attrs_.is_first_install());
  EXPECT_TRUE(install_attrs_.is_invalid());
  EXPECT_FALSE(install_attrs_.is_initialized());

  // Should be empty.
  EXPECT_EQ(0, install_attrs_.Count());
}

TEST_F(InstallAttributesTest, LegacyBoot) {
  // Check the baseline.
  EXPECT_EQ(false, install_attrs_.is_first_install());
  EXPECT_EQ(false, install_attrs_.is_initialized());
  EXPECT_EQ(false, install_attrs_.is_invalid());

  Lockbox::ErrorId error_id = Lockbox::kErrorIdNoNvramSpace;
  EXPECT_CALL(lockbox_, Load(_))
   .Times(1)
   .WillOnce(DoAll(SetArgumentPointee<0>(error_id), Return(false)));
  EXPECT_TRUE(install_attrs_.Init());
  EXPECT_FALSE(install_attrs_.is_first_install());
  EXPECT_FALSE(install_attrs_.is_invalid());
  EXPECT_TRUE(install_attrs_.is_initialized());

  // Should be empty.
  EXPECT_EQ(0, install_attrs_.Count());
}

} // namespace cryptohome
