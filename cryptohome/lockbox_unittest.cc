// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for Lockbox.

#include "lockbox.h"

#include <base/file_util.h>
#include <base/logging.h>
#include <chromeos/utility.h>
#include <gtest/gtest.h>
#include <vector>

#include "mock_lockbox.h"
#include "mock_platform.h"
#include "mock_tpm.h"
#include "secure_blob.h"

namespace cryptohome {
using std::string;
using ::testing::_;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::InSequence;
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
class LockboxTest : public ::testing::Test {
 public:
  LockboxTest() :
    lockbox_(NULL, 0xdeadbeef) { }
  virtual ~LockboxTest() { }

  virtual void SetUp() {
    // Create the OOBE data to reuse for post-boot tests.
    // This generates the expected NVRAM value and serialized file data.
    file_data_.assign(kFileData, kFileData + strlen(kFileData));
    lockbox_.set_tpm(&tpm_);
  }

  virtual void TearDown() { }

  void DoStore(Lockbox* lockbox, SecureBlob* nvram_data) {
    lockbox->set_tpm(&tpm_);
    Lockbox::ErrorId error;

    // Ensure a connected, owned TPM.
    EXPECT_CALL(tpm_, IsConnected())
      .Times(1)
      .WillRepeatedly(Return(true));
    EXPECT_CALL(tpm_, IsOwned())
      .Times(1)
      .WillRepeatedly(Return(true));
    chromeos::Blob salt(Lockbox::kReservedSaltBytes, 'A');
    EXPECT_CALL(tpm_, GetRandomData(Lockbox::kReservedSaltBytes, _))
      .Times(1)
      .WillRepeatedly(DoAll(SetArgumentPointee<1>(salt), Return(true)));

    // Destory calls with no file or existing NVRAM space.
    EXPECT_CALL(tpm_, IsNvramDefined(0xdeadbeef))
      .WillOnce(Return(true));
    {
      InSequence s;
      EXPECT_CALL(tpm_, IsNvramLocked(0xdeadbeef))
        .WillOnce(Return(false));
      EXPECT_CALL(tpm_, WriteNvram(0xdeadbeef, _))
        .Times(1)
        .WillOnce(DoAll(SaveArg<1>(nvram_data), Return(true)));
      // size==0 locks.
      chromeos::Blob empty_data(0);
      EXPECT_CALL(tpm_, WriteNvram(0xdeadbeef, Eq(empty_data)))
        .Times(1)
        .WillRepeatedly(Return(true));
      EXPECT_CALL(tpm_, IsNvramLocked(0xdeadbeef))
        .Times(1)
        .WillOnce(Return(true));
    }
    EXPECT_TRUE(lockbox->Store(file_data_, &error));
  }

  void GenerateNvramData(SecureBlob* nvram) {
    Lockbox throwaway_lockbox(NULL, 0xdeadbeef);
    DoStore(&throwaway_lockbox, nvram);
    Mock::VerifyAndClearExpectations(&tpm_);
  }

  static const char* kFileData;
  Lockbox lockbox_;
  Crypto crypto_;
  NiceMock<MockTpm> tpm_;
  chromeos::Blob file_data_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LockboxTest);
};
const char* LockboxTest::kFileData = "42";


//
// The actual tests!
//

// First install on a system ever.
TEST_F(LockboxTest, CreateFirstInstall) {
  Lockbox::ErrorId error;

  // Ensure a connected, owned-this-time TPM.
  EXPECT_CALL(tpm_, IsConnected())
    .Times(2)
    .WillRepeatedly(Return(true));
  EXPECT_CALL(tpm_, IsOwned())
    .Times(2)
    .WillRepeatedly(Return(true));
  static const char* kOwnerPassword = "sup";
  chromeos::Blob pw;
  pw.assign(kOwnerPassword, kOwnerPassword + strlen(kOwnerPassword));
  EXPECT_CALL(tpm_, GetOwnerPassword(_))
    .Times(2)
    .WillRepeatedly(DoAll(SetArgumentPointee<0>(pw), Return(true)));

  // Destory calls with no file or existing NVRAM space.
  EXPECT_CALL(tpm_, IsNvramDefined(0xdeadbeef))
    .WillOnce(Return(false));

  // Create the new space.
  EXPECT_CALL(tpm_,
              DefineLockOnceNvram(0xdeadbeef, Lockbox::kReservedNvramBytes, 0))
    .WillOnce(Return(true));
  EXPECT_TRUE(lockbox_.Create(&error));
}

TEST_F(LockboxTest, CreateOnReinstallWithFullAuth) {
  Lockbox::ErrorId error;

  // Ensure a connected, owned-this-time TPM.
  EXPECT_CALL(tpm_, IsConnected())
    .Times(2)
    .WillRepeatedly(Return(true));
  EXPECT_CALL(tpm_, IsOwned())
    .Times(2)
    .WillRepeatedly(Return(true));
  static const char* kOwnerPassword = "sup";
  chromeos::Blob pw;
  pw.assign(kOwnerPassword, kOwnerPassword + strlen(kOwnerPassword));
  EXPECT_CALL(tpm_, GetOwnerPassword(_))
    .Times(2)
    .WillRepeatedly(DoAll(SetArgumentPointee<0>(pw), Return(true)));

  // Destory calls with no file or existing NVRAM space.
  EXPECT_CALL(tpm_, IsNvramDefined(0xdeadbeef))
    .WillOnce(Return(true));
  EXPECT_CALL(tpm_, DestroyNvram(0xdeadbeef))
    .WillOnce(Return(true));

  // Create the new space.
  EXPECT_CALL(tpm_,
              DefineLockOnceNvram(0xdeadbeef, Lockbox::kReservedNvramBytes, 0))
    .WillOnce(Return(true));
  EXPECT_TRUE(lockbox_.Create(&error));
}

TEST_F(LockboxTest, CreateWithNoAuth) {
  Lockbox::ErrorId error;

  // Ensure a connected, owned-this-time TPM.
  EXPECT_CALL(tpm_, IsConnected())
    .Times(1)
    .WillRepeatedly(Return(true));
  EXPECT_CALL(tpm_, IsOwned())
    .Times(1)
    .WillRepeatedly(Return(true));
  EXPECT_CALL(tpm_, GetOwnerPassword(_))
    .Times(1)
    .WillRepeatedly(Return(false));
  EXPECT_FALSE(lockbox_.Create(&error));
}

TEST_F(LockboxTest, DestroyPristine) {
  Lockbox::ErrorId error;

  EXPECT_CALL(tpm_, IsConnected())
    .Times(1)
    .WillRepeatedly(Return(true));
  EXPECT_CALL(tpm_, IsOwned())
    .Times(1)
    .WillRepeatedly(Return(true));
  static const char* kOwnerPassword = "sup";
  chromeos::Blob pw;
  pw.assign(kOwnerPassword, kOwnerPassword + strlen(kOwnerPassword));
  EXPECT_CALL(tpm_, GetOwnerPassword(_))
    .Times(1)
    .WillRepeatedly(DoAll(SetArgumentPointee<0>(pw), Return(true)));

  // Destory calls with no file or existing NVRAM space.
  EXPECT_CALL(tpm_, IsNvramDefined(0xdeadbeef))
    .WillOnce(Return(false));

  EXPECT_TRUE(lockbox_.Destroy(&error));
}

TEST_F(LockboxTest, DestroyWithOldData) {
  Lockbox::ErrorId error;

  EXPECT_CALL(tpm_, IsConnected())
    .Times(1)
    .WillRepeatedly(Return(true));
  EXPECT_CALL(tpm_, IsOwned())
    .Times(1)
    .WillRepeatedly(Return(true));
  static const char* kOwnerPassword = "sup";
  chromeos::Blob pw;
  pw.assign(kOwnerPassword, kOwnerPassword + strlen(kOwnerPassword));
  EXPECT_CALL(tpm_, GetOwnerPassword(_))
    .Times(1)
    .WillRepeatedly(DoAll(SetArgumentPointee<0>(pw), Return(true)));

  // Destory calls with no file or existing NVRAM space.
  EXPECT_CALL(tpm_, IsNvramDefined(0xdeadbeef))
    .WillOnce(Return(true));
  EXPECT_CALL(tpm_, DestroyNvram(0xdeadbeef))
    .WillOnce(Return(true));

  EXPECT_TRUE(lockbox_.Destroy(&error));
}

TEST_F(LockboxTest, StoreOk) {
  SecureBlob nvram_data;
  DoStore(&lockbox_, &nvram_data);
}

TEST_F(LockboxTest, StoreLockedNvram) {
  Lockbox::ErrorId error;

  // Ensure a connected, owned TPM.
  EXPECT_CALL(tpm_, IsConnected())
    .Times(1)
    .WillRepeatedly(Return(true));
  EXPECT_CALL(tpm_, IsOwned())
    .Times(1)
    .WillRepeatedly(Return(true));
  EXPECT_CALL(tpm_, IsNvramDefined(0xdeadbeef))
    .WillOnce(Return(true));
  chromeos::Blob salt(7, 'A');
  EXPECT_CALL(tpm_, GetRandomData(Lockbox::kReservedSaltBytes, _))
    .Times(1)
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(salt), Return(true)));
  EXPECT_CALL(tpm_, IsNvramLocked(0xdeadbeef))
    .WillOnce(Return(true));
  EXPECT_FALSE(lockbox_.Store(file_data_, &error));
  EXPECT_EQ(error, Lockbox::kErrorIdNvramInvalid);
}

TEST_F(LockboxTest, StoreNoNvram) {
  lockbox_.set_tpm(&tpm_);
  Lockbox::ErrorId error;

  // Ensure a connected, owned TPM.
  EXPECT_CALL(tpm_, IsConnected())
    .Times(1)
    .WillRepeatedly(Return(true));
  EXPECT_CALL(tpm_, IsOwned())
    .Times(1)
    .WillRepeatedly(Return(true));

  chromeos::Blob salt(7, 'A');
  EXPECT_CALL(tpm_, GetRandomData(Lockbox::kReservedSaltBytes, _))
    .Times(1)
    .WillRepeatedly(DoAll(SetArgumentPointee<1>(salt), Return(true)));

  EXPECT_CALL(tpm_, IsNvramDefined(0xdeadbeef))
    .WillOnce(Return(false));
  EXPECT_FALSE(lockbox_.Store(file_data_, &error));
  EXPECT_EQ(error, Lockbox::kErrorIdNoNvramSpace);
}

TEST_F(LockboxTest, StoreTpmNotReady) {
  Lockbox::ErrorId error;

  // Ensure a connected, owned TPM.
  EXPECT_CALL(tpm_, IsConnected())
    .Times(1)
    .WillRepeatedly(Return(true));
  EXPECT_CALL(tpm_, IsOwned())
    .Times(1)
    .WillRepeatedly(Return(false));
  EXPECT_FALSE(lockbox_.Store(file_data_, &error));
  EXPECT_EQ(error, Lockbox::kErrorIdTpmError);
}

TEST_F(LockboxTest, LoadAndVerifyOkTpm) {
  SecureBlob nvram_data(0);
  GenerateNvramData(&nvram_data);

  // Ensure a connected, owned TPM.
  EXPECT_CALL(tpm_, IsConnected())
    .Times(1)
    .WillRepeatedly(Return(true));
  EXPECT_CALL(tpm_, IsOwned())
    .Times(1)
    .WillRepeatedly(Return(true));
  EXPECT_CALL(tpm_, IsNvramDefined(0xdeadbeef))
    .WillOnce(Return(true));
  EXPECT_CALL(tpm_, IsNvramLocked(0xdeadbeef))
    .WillOnce(Return(true));
  EXPECT_CALL(tpm_, ReadNvram(0xdeadbeef, _))
    .Times(1)
    .WillOnce(DoAll(SetArgumentPointee<1>(nvram_data), Return(true)));

  Lockbox::ErrorId error;
  EXPECT_TRUE(lockbox_.Load(&error));
  EXPECT_TRUE(lockbox_.Verify(file_data_, &error));
}

TEST_F(LockboxTest, LoadAndVerifyBadSize) {
  SecureBlob nvram_data(0);
  GenerateNvramData(&nvram_data);

  // Ensure a connected, owned TPM.
  EXPECT_CALL(tpm_, IsConnected())
    .Times(1)
    .WillRepeatedly(Return(true));
  EXPECT_CALL(tpm_, IsOwned())
    .Times(1)
    .WillRepeatedly(Return(true));
  EXPECT_CALL(tpm_, IsNvramDefined(0xdeadbeef))
    .WillOnce(Return(true));
  EXPECT_CALL(tpm_, IsNvramLocked(0xdeadbeef))
    .WillOnce(Return(true));
  // Change the expected file size to 0.
  nvram_data[0] = 0;
  nvram_data[1] = 0;
  nvram_data[2] = 0;
  nvram_data[3] = 0;
  EXPECT_CALL(tpm_, ReadNvram(0xdeadbeef, _))
    .Times(1)
    .WillOnce(DoAll(SetArgumentPointee<1>(nvram_data), Return(true)));

  Lockbox::ErrorId error;
  EXPECT_TRUE(lockbox_.Load(&error));
  EXPECT_FALSE(lockbox_.Verify(file_data_, &error));
  EXPECT_EQ(error, Lockbox::kErrorIdSizeMismatch);
}

TEST_F(LockboxTest, LoadAndVerifyBadHash) {
  SecureBlob nvram_data(0);
  GenerateNvramData(&nvram_data);

  // Ensure a connected, owned TPM.
  EXPECT_CALL(tpm_, IsConnected())
    .Times(1)
    .WillRepeatedly(Return(true));
  EXPECT_CALL(tpm_, IsOwned())
    .Times(1)
    .WillRepeatedly(Return(true));
  EXPECT_CALL(tpm_, IsNvramDefined(0xdeadbeef))
    .WillOnce(Return(true));
  EXPECT_CALL(tpm_, IsNvramLocked(0xdeadbeef))
    .WillOnce(Return(true));
  // Truncate the hash.
  nvram_data.resize(nvram_data.size() - Lockbox::kReservedDigestBytes);
  // Fill with 0s.
  nvram_data.resize(Lockbox::kReservedNvramBytes);
  EXPECT_CALL(tpm_, ReadNvram(0xdeadbeef, _))
    .Times(1)
    .WillOnce(DoAll(SetArgumentPointee<1>(nvram_data), Return(true)));

  Lockbox::ErrorId error;
  EXPECT_TRUE(lockbox_.Load(&error));
  EXPECT_FALSE(lockbox_.Verify(file_data_, &error));
  EXPECT_EQ(Lockbox::kErrorIdHashMismatch, error);
}

TEST_F(LockboxTest, LoadAndVerifyBadData) {
  SecureBlob nvram_data(0);
  GenerateNvramData(&nvram_data);

  // Ensure a connected, owned TPM.
  EXPECT_CALL(tpm_, IsConnected())
    .Times(1)
    .WillRepeatedly(Return(true));
  EXPECT_CALL(tpm_, IsOwned())
    .Times(1)
    .WillRepeatedly(Return(true));
  EXPECT_CALL(tpm_, IsNvramDefined(0xdeadbeef))
    .WillOnce(Return(true));
  EXPECT_CALL(tpm_, IsNvramLocked(0xdeadbeef))
    .WillOnce(Return(true));
  EXPECT_CALL(tpm_, ReadNvram(0xdeadbeef, _))
    .Times(1)
    .WillOnce(DoAll(SetArgumentPointee<1>(nvram_data), Return(true)));

  Lockbox::ErrorId error;
  EXPECT_TRUE(lockbox_.Load(&error));
  // Insert bad data.
  file_data_[0] = 0;
  EXPECT_FALSE(lockbox_.Verify(file_data_, &error));
}

}  // namespace cryptohome
