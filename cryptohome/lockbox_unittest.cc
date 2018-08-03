// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for Lockbox.

#include "cryptohome/lockbox.h"

#include <base/files/file_util.h>
#include <base/logging.h>
#include <brillo/process_mock.h>
#include <brillo/secure_blob.h>
#include <gtest/gtest.h>

#include "cryptohome/mock_lockbox.h"
#include "cryptohome/mock_platform.h"
#include "cryptohome/mock_tpm.h"

namespace cryptohome {
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SetArgPointee;
using ::testing::_;
using brillo::SecureBlob;

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

  // Perform an NVRAM store.
  // lockbox: Lockbox object to operate on.
  // nvram_data: the Blob of data to store into NVRAM.
  // nvram_version: the default NVRAM version layout for Lockbox object.
  // defined_nvram_size: the size of the defined NVRAM to test V2->V1 code.
  void DoStore(Lockbox* lockbox, SecureBlob* nvram_data,
               uint32_t nvram_version, uint32_t defined_nvram_size) {
    uint32_t salt_size;
    const char *salt_hash;

    if (defined_nvram_size == Lockbox::kReservedNvramBytesV1) {
      salt_size = Lockbox::kReservedSaltBytesV1;
      // sha256 of entire V1 lockbox NVRAM area.
      salt_hash =
          "f5f68c0c7ea1ccddc742b4b690e7c0ded9be59d33bcd56f9c7a7f7b273044a82";
    } else {
      salt_size = Lockbox::kReservedSaltBytesV2;
      // sha256 of 32 'A's.
      salt_hash =
          "22a48051594c1949deed7040850c1f0f8764537f5191be56732d16a54c1d8153";
    }

    lockbox->set_tpm(&tpm_);
    lockbox->set_nvram_version(nvram_version);
    lockbox->set_process(&process_);
    LockboxError error;

    // Ensure an enabled, owned TPM.
    EXPECT_CALL(tpm_, IsEnabled())
      .Times(1)
      .WillRepeatedly(Return(true));
    EXPECT_CALL(tpm_, IsOwned())
      .Times(1)
      .WillRepeatedly(Return(true));

    // Destory calls with no file or existing NVRAM space.
    EXPECT_CALL(tpm_, IsNvramDefined(0xdeadbeef))
      .WillOnce(Return(true));
    {
      InSequence s;
      EXPECT_CALL(tpm_, IsNvramLocked(0xdeadbeef))
        .WillOnce(Return(false));
      EXPECT_CALL(tpm_, GetNvramSize(0xdeadbeef))
        .WillOnce(Return(defined_nvram_size));
      brillo::Blob salt(salt_size, 'A');
      EXPECT_CALL(tpm_, GetRandomDataBlob(salt_size, _))
        .Times(1)
        .WillRepeatedly(DoAll(SetArgPointee<1>(salt), Return(true)));
      EXPECT_CALL(tpm_, WriteNvram(0xdeadbeef, _))
        .Times(1)
        .WillOnce(DoAll(SaveArg<1>(nvram_data), Return(true)));
      EXPECT_CALL(tpm_, WriteLockNvram(0xdeadbeef))
        .Times(1)
        .WillRepeatedly(Return(true));
      EXPECT_CALL(tpm_, IsNvramLocked(0xdeadbeef))
        .Times(1)
        .WillOnce(Return(true));
    }
    EXPECT_CALL(process_, Reset(0)).Times(1);
    EXPECT_CALL(process_, AddArg("/usr/sbin/mount-encrypted")).Times(1);
    EXPECT_CALL(process_, AddArg("finalize")).Times(1);
    EXPECT_CALL(process_, AddArg(salt_hash)).Times(1);
    EXPECT_CALL(process_, BindFd(_, 1)).Times(1);
    EXPECT_CALL(process_, BindFd(_, 2)).Times(1);
    EXPECT_CALL(process_, Run()).Times(1).WillOnce(Return(0));
    EXPECT_TRUE(lockbox->Store(file_data_, &error));
  }

  // Populate the mock NVRAM with valid data.
  void GenerateNvramData(SecureBlob* nvram, uint32_t version,
                         uint32_t defined_nvram_size) {
    Lockbox throwaway_lockbox(NULL, 0xdeadbeef);
    DoStore(&throwaway_lockbox, nvram, version, defined_nvram_size);
    Mock::VerifyAndClearExpectations(&tpm_);
  }

  static const char* kFileData;
  Lockbox lockbox_;
  NiceMock<MockTpm> tpm_;
  NiceMock<brillo::ProcessMock> process_;
  brillo::Blob file_data_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LockboxTest);
};
const char* LockboxTest::kFileData = "42";


//
// The actual tests!
//

// First install on a system ever.
TEST_F(LockboxTest, CreateFirstInstall) {
  LockboxError error;

  // Ensure an enabled, owned-this-time TPM.
  EXPECT_CALL(tpm_, IsEnabled())
    .Times(2)
    .WillRepeatedly(Return(true));
  EXPECT_CALL(tpm_, IsOwned())
    .Times(2)
    .WillRepeatedly(Return(true));
  static const char* kOwnerPassword = "sup";
  brillo::Blob pw;
  pw.assign(kOwnerPassword, kOwnerPassword + strlen(kOwnerPassword));
  EXPECT_CALL(tpm_, GetOwnerPassword(_))
    .Times(2)
    .WillRepeatedly(DoAll(SetArgPointee<0>(pw), Return(true)));

  // Destory calls with no file or existing NVRAM space.
  EXPECT_CALL(tpm_, IsNvramDefined(0xdeadbeef))
    .WillOnce(Return(false));

  // Create the new space.
  EXPECT_CALL(tpm_,
              DefineNvram(0xdeadbeef, Lockbox::kReservedNvramBytesV2,
                          Tpm::kTpmNvramWriteDefine |
                          Tpm::kTpmNvramBindToPCR0))
    .WillOnce(Return(true));
  EXPECT_TRUE(lockbox_.Create(&error));
}

TEST_F(LockboxTest, CreateOnReinstallWithFullAuth) {
  LockboxError error;

  // Ensure an enabled, owned-this-time TPM.
  EXPECT_CALL(tpm_, IsEnabled())
    .Times(2)
    .WillRepeatedly(Return(true));
  EXPECT_CALL(tpm_, IsOwned())
    .Times(2)
    .WillRepeatedly(Return(true));
  static const char* kOwnerPassword = "sup";
  brillo::Blob pw;
  pw.assign(kOwnerPassword, kOwnerPassword + strlen(kOwnerPassword));
  EXPECT_CALL(tpm_, GetOwnerPassword(_))
    .Times(2)
    .WillRepeatedly(DoAll(SetArgPointee<0>(pw), Return(true)));

  // Destory calls with no file or existing NVRAM space.
  EXPECT_CALL(tpm_, IsNvramDefined(0xdeadbeef))
    .WillOnce(Return(true));
  EXPECT_CALL(tpm_, DestroyNvram(0xdeadbeef))
    .WillOnce(Return(true));

  // Create the new space.
  EXPECT_CALL(tpm_,
              DefineNvram(0xdeadbeef, Lockbox::kReservedNvramBytesV2,
                          Tpm::kTpmNvramWriteDefine |
                          Tpm::kTpmNvramBindToPCR0))
    .WillOnce(Return(true));
  EXPECT_TRUE(lockbox_.Create(&error));
}

TEST_F(LockboxTest, CreateWithNoAuth) {
  LockboxError error;

  // Ensure an enabled, owned-this-time TPM.
  EXPECT_CALL(tpm_, IsEnabled())
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
  LockboxError error;

  EXPECT_CALL(tpm_, IsEnabled())
    .Times(1)
    .WillRepeatedly(Return(true));
  EXPECT_CALL(tpm_, IsOwned())
    .Times(1)
    .WillRepeatedly(Return(true));
  static const char* kOwnerPassword = "sup";
  brillo::Blob pw;
  pw.assign(kOwnerPassword, kOwnerPassword + strlen(kOwnerPassword));
  EXPECT_CALL(tpm_, GetOwnerPassword(_))
    .Times(1)
    .WillRepeatedly(DoAll(SetArgPointee<0>(pw), Return(true)));

  // Destory calls with no file or existing NVRAM space.
  EXPECT_CALL(tpm_, IsNvramDefined(0xdeadbeef))
    .WillOnce(Return(false));

  EXPECT_TRUE(lockbox_.Destroy(&error));
}

TEST_F(LockboxTest, DestroyWithOldData) {
  LockboxError error;

  EXPECT_CALL(tpm_, IsEnabled())
    .Times(1)
    .WillRepeatedly(Return(true));
  EXPECT_CALL(tpm_, IsOwned())
    .Times(1)
    .WillRepeatedly(Return(true));
  static const char* kOwnerPassword = "sup";
  brillo::Blob pw;
  pw.assign(kOwnerPassword, kOwnerPassword + strlen(kOwnerPassword));
  EXPECT_CALL(tpm_, GetOwnerPassword(_))
    .Times(1)
    .WillRepeatedly(DoAll(SetArgPointee<0>(pw), Return(true)));

  // Destory calls with no file or existing NVRAM space.
  EXPECT_CALL(tpm_, IsNvramDefined(0xdeadbeef))
    .WillOnce(Return(true));
  EXPECT_CALL(tpm_, DestroyNvram(0xdeadbeef))
    .WillOnce(Return(true));

  EXPECT_TRUE(lockbox_.Destroy(&error));
}

TEST_F(LockboxTest, StoreOk) {
  SecureBlob nvram_data;
  DoStore(&lockbox_, &nvram_data, Lockbox::kNvramVersionDefault,
          Lockbox::kReservedNvramBytesV2);
}

TEST_F(LockboxTest, StoreLockedNvram) {
  LockboxError error;

  // Ensure an enabled, owned TPM.
  EXPECT_CALL(tpm_, IsEnabled())
    .Times(1)
    .WillRepeatedly(Return(true));
  EXPECT_CALL(tpm_, IsOwned())
    .Times(1)
    .WillRepeatedly(Return(true));
  EXPECT_CALL(tpm_, IsNvramDefined(0xdeadbeef))
    .WillOnce(Return(true));
  EXPECT_CALL(tpm_, IsNvramLocked(0xdeadbeef))
    .WillOnce(Return(true));
  EXPECT_FALSE(lockbox_.Store(file_data_, &error));
  EXPECT_EQ(error, LockboxError::kNvramInvalid);
}

TEST_F(LockboxTest, StoreUnlockedNvramSizeBad) {
  LockboxError error;

  // Ensure an enabled, owned TPM.
  EXPECT_CALL(tpm_, IsEnabled())
    .Times(1)
    .WillRepeatedly(Return(true));
  EXPECT_CALL(tpm_, IsOwned())
    .Times(1)
    .WillRepeatedly(Return(true));
  EXPECT_CALL(tpm_, IsNvramDefined(0xdeadbeef))
    .WillOnce(Return(true));
  EXPECT_CALL(tpm_, IsNvramLocked(0xdeadbeef))
    .WillOnce(Return(false));
  // Return a bad NVRAM size.
  EXPECT_CALL(tpm_, GetNvramSize(0xdeadbeef))
    .WillOnce(Return(0));
  EXPECT_FALSE(lockbox_.Store(file_data_, &error));
  EXPECT_EQ(error, LockboxError::kNvramInvalid);
}

TEST_F(LockboxTest, StoreNoNvram) {
  lockbox_.set_tpm(&tpm_);
  LockboxError error;

  // Ensure an enabled, owned TPM.
  EXPECT_CALL(tpm_, IsEnabled())
    .Times(1)
    .WillRepeatedly(Return(true));
  EXPECT_CALL(tpm_, IsOwned())
    .Times(1)
    .WillRepeatedly(Return(true));

  EXPECT_CALL(tpm_, IsNvramDefined(0xdeadbeef))
    .WillOnce(Return(false));
  EXPECT_FALSE(lockbox_.Store(file_data_, &error));
  EXPECT_EQ(error, LockboxError::kNoNvramSpace);
}

TEST_F(LockboxTest, StoreTpmNotReady) {
  LockboxError error;

  // Ensure an enabled, owned TPM.
  EXPECT_CALL(tpm_, IsEnabled())
    .Times(1)
    .WillRepeatedly(Return(true));
  EXPECT_CALL(tpm_, IsOwned())
    .Times(1)
    .WillRepeatedly(Return(false));
  EXPECT_FALSE(lockbox_.Store(file_data_, &error));
  EXPECT_EQ(error, LockboxError::kTpmError);
}

TEST_F(LockboxTest, LoadAndVerifyOkTpmDefault) {
  SecureBlob nvram_data(0);
  GenerateNvramData(&nvram_data, Lockbox::kNvramVersionDefault,
                    Lockbox::kReservedNvramBytesV2);

  // Ensure an enabled, owned TPM.
  EXPECT_CALL(tpm_, IsEnabled())
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
    .WillOnce(DoAll(SetArgPointee<1>(nvram_data), Return(true)));

  LockboxError error;
  EXPECT_TRUE(lockbox_.Load(&error));
  EXPECT_TRUE(lockbox_.Verify(file_data_, &error));
}

TEST_F(LockboxTest, LoadAndVerifyOkTpmV1) {
  SecureBlob nvram_data(0);
  GenerateNvramData(&nvram_data, Lockbox::kNvramVersion1,
                    Lockbox::kReservedNvramBytesV1);

  // Ensure an enabled, owned TPM.
  EXPECT_CALL(tpm_, IsEnabled())
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
    .WillOnce(DoAll(SetArgPointee<1>(nvram_data), Return(true)));

  LockboxError error;
  EXPECT_TRUE(lockbox_.Load(&error));
  EXPECT_TRUE(lockbox_.Verify(file_data_, &error));
}

TEST_F(LockboxTest, LoadAndVerifyOkTpmV2) {
  SecureBlob nvram_data(0);
  GenerateNvramData(&nvram_data, Lockbox::kNvramVersion2,
                    Lockbox::kReservedNvramBytesV2);

  // Ensure an enabled, owned TPM.
  EXPECT_CALL(tpm_, IsEnabled())
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
    .WillOnce(DoAll(SetArgPointee<1>(nvram_data), Return(true)));

  LockboxError error;
  EXPECT_TRUE(lockbox_.Load(&error));
  EXPECT_TRUE(lockbox_.Verify(file_data_, &error));
}

TEST_F(LockboxTest, LoadAndVerifyOkTpmV2Downgrade) {
  SecureBlob nvram_data(0);
  GenerateNvramData(&nvram_data, Lockbox::kNvramVersionDefault,
                    Lockbox::kReservedNvramBytesV1);

  // Ensure an enabled, owned TPM.
  EXPECT_CALL(tpm_, IsEnabled())
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
    .WillOnce(DoAll(SetArgPointee<1>(nvram_data), Return(true)));

  LockboxError error;
  EXPECT_TRUE(lockbox_.Load(&error));
  EXPECT_TRUE(lockbox_.Verify(file_data_, &error));
}

TEST_F(LockboxTest, LoadAndVerifyBadSize) {
  SecureBlob nvram_data(0);
  GenerateNvramData(&nvram_data, Lockbox::kNvramVersionDefault,
                    Lockbox::kReservedNvramBytesV2);

  // Ensure an enabled, owned TPM.
  EXPECT_CALL(tpm_, IsEnabled())
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
    .WillOnce(DoAll(SetArgPointee<1>(nvram_data), Return(true)));

  LockboxError error;
  EXPECT_TRUE(lockbox_.Load(&error));
  EXPECT_FALSE(lockbox_.Verify(file_data_, &error));
  EXPECT_EQ(error, LockboxError::kSizeMismatch);
}

TEST_F(LockboxTest, LoadAndVerifyBadHash) {
  SecureBlob nvram_data(0);
  GenerateNvramData(&nvram_data, Lockbox::kNvramVersionDefault,
                    Lockbox::kReservedNvramBytesV2);

  // Ensure an enabled, owned TPM.
  EXPECT_CALL(tpm_, IsEnabled())
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
  nvram_data.resize(Lockbox::kReservedNvramBytesV2);
  EXPECT_CALL(tpm_, ReadNvram(0xdeadbeef, _))
    .Times(1)
    .WillOnce(DoAll(SetArgPointee<1>(nvram_data), Return(true)));

  LockboxError error;
  EXPECT_TRUE(lockbox_.Load(&error));
  EXPECT_FALSE(lockbox_.Verify(file_data_, &error));
  EXPECT_EQ(LockboxError::kHashMismatch, error);
}

TEST_F(LockboxTest, LoadAndVerifyBadData) {
  SecureBlob nvram_data(0);
  GenerateNvramData(&nvram_data, Lockbox::kNvramVersionDefault,
                    Lockbox::kReservedNvramBytesV2);

  // Ensure an enabled, owned TPM.
  EXPECT_CALL(tpm_, IsEnabled())
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
    .WillOnce(DoAll(SetArgPointee<1>(nvram_data), Return(true)));

  LockboxError error;
  EXPECT_TRUE(lockbox_.Load(&error));
  // Insert bad data.
  file_data_[0] = 0;
  EXPECT_FALSE(lockbox_.Verify(file_data_, &error));
}

}  // namespace cryptohome
