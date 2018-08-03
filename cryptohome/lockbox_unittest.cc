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

#include "cryptohome/cryptolib.h"
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
  LockboxTest() : lockbox_(NULL, 0xdeadbeef) {}
  ~LockboxTest() override = default;

  void SetUp() override {
    // Create the OOBE data to reuse for post-boot tests.
    // This generates the expected NVRAM value and serialized file data.
    file_data_.assign(kFileData, kFileData + strlen(kFileData));
    lockbox_.set_tpm(&tpm_);
    lockbox_.set_process(&process_);
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
              DefineNvram(0xdeadbeef,
                          LockboxContents::GetNvramSize(NvramVersion::kDefault),
                          Tpm::kTpmNvramWriteDefine | Tpm::kTpmNvramBindToPCR0))
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
              DefineNvram(0xdeadbeef,
                          LockboxContents::GetNvramSize(NvramVersion::kDefault),
                          Tpm::kTpmNvramWriteDefine | Tpm::kTpmNvramBindToPCR0))
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
  LockboxError error;
  brillo::SecureBlob salt(static_cast<size_t>(NvramVersion::kDefault), 'A');

  // Ensure an enabled, owned TPM.
  EXPECT_CALL(tpm_, IsEnabled())
    .WillRepeatedly(Return(true));
  EXPECT_CALL(tpm_, IsOwned())
    .WillRepeatedly(Return(true));

  // Destory calls with no file or existing NVRAM space.
  EXPECT_CALL(tpm_, IsNvramDefined(0xdeadbeef))
    .WillOnce(Return(true));
  {
    InSequence s;
    EXPECT_CALL(tpm_, IsNvramLocked(0xdeadbeef))
      .WillOnce(Return(false));
    EXPECT_CALL(tpm_, GetNvramSize(0xdeadbeef))
      .WillOnce(Return(LockboxContents::GetNvramSize(NvramVersion::kDefault)));
    EXPECT_CALL(tpm_, GetRandomDataBlob(salt.size(), _))
      .WillRepeatedly(DoAll(SetArgPointee<1>(salt), Return(true)));
    EXPECT_CALL(tpm_, WriteNvram(0xdeadbeef, _))
      .WillOnce(Return(true));
    EXPECT_CALL(tpm_, WriteLockNvram(0xdeadbeef))
      .WillRepeatedly(Return(true));
    EXPECT_CALL(tpm_, IsNvramLocked(0xdeadbeef))
      .WillOnce(Return(true));
  }
  EXPECT_CALL(process_, Reset(0)).Times(1);
  EXPECT_CALL(process_, AddArg("/usr/sbin/mount-encrypted")).Times(1);
  EXPECT_CALL(process_, AddArg("finalize")).Times(1);
  EXPECT_CALL(process_, AddArg(CryptoLib::BlobToHex(CryptoLib::Sha256(salt))))
      .Times(1);
  EXPECT_CALL(process_, BindFd(_, 1)).Times(1);
  EXPECT_CALL(process_, BindFd(_, 2)).Times(1);
  EXPECT_CALL(process_, Run()).Times(1).WillOnce(Return(0));

  EXPECT_TRUE(lockbox_.Store(file_data_, &error));
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

class LockboxContentsTest : public testing::Test {
 public:
  LockboxContentsTest() = default;

  void GenerateNvramData(NvramVersion version, brillo::SecureBlob* nvram_data) {
    std::unique_ptr<LockboxContents> contents =
        LockboxContents::New(LockboxContents::GetNvramSize(version));
    ASSERT_TRUE(contents);
    ASSERT_TRUE(contents->SetKeyMaterial(
        brillo::SecureBlob(contents->key_material_size(), 'A')));
    ASSERT_TRUE(contents->Protect({42}));
    ASSERT_TRUE(contents->Encode(nvram_data));
  }

  void LoadAndVerify(const brillo::SecureBlob& nvram_data,
                     const brillo::Blob& data,
                     LockboxError expected_error) {
    std::unique_ptr<LockboxContents> contents =
        LockboxContents::New(nvram_data.size());
    ASSERT_TRUE(contents);
    ASSERT_TRUE(contents->Decode(nvram_data));
    LockboxError error = LockboxError::kNone;
    EXPECT_EQ(expected_error == LockboxError::kNone,
              contents->Verify(data, &error));
    EXPECT_EQ(expected_error, error);
  }
};

TEST_F(LockboxContentsTest, LoadAndVerifyOkTpmDefault) {
  brillo::SecureBlob nvram_data;
  ASSERT_NO_FATAL_FAILURE(
      GenerateNvramData(NvramVersion::kDefault, &nvram_data));
  LoadAndVerify(nvram_data, {42}, LockboxError::kNone);
}

TEST_F(LockboxContentsTest, LoadAndVerifyOkTpmV1) {
  brillo::SecureBlob nvram_data;
  ASSERT_NO_FATAL_FAILURE(
      GenerateNvramData(NvramVersion::kVersion1, &nvram_data));
  LoadAndVerify(nvram_data, {42}, LockboxError::kNone);
}

TEST_F(LockboxContentsTest, LoadAndVerifyOkTpmV2) {
  brillo::SecureBlob nvram_data;
  ASSERT_NO_FATAL_FAILURE(
      GenerateNvramData(NvramVersion::kVersion2, &nvram_data));
  LoadAndVerify(nvram_data, {42}, LockboxError::kNone);
}

TEST_F(LockboxContentsTest, LoadAndVerifyBadSize) {
  SecureBlob nvram_data;
  ASSERT_NO_FATAL_FAILURE(
      GenerateNvramData(NvramVersion::kVersion2, &nvram_data));

  // Change the expected file size to 0.
  nvram_data[0] = 0;
  nvram_data[1] = 0;
  nvram_data[2] = 0;
  nvram_data[3] = 0;

  LoadAndVerify(nvram_data, {42}, LockboxError::kSizeMismatch);
}

TEST_F(LockboxContentsTest, LoadAndVerifyBadHash) {
  SecureBlob nvram_data;
  ASSERT_NO_FATAL_FAILURE(
      GenerateNvramData(NvramVersion::kVersion2, &nvram_data));

  // Invalidate the hash.
  nvram_data.back() ^= 0xff;

  LoadAndVerify(nvram_data, {42}, LockboxError::kHashMismatch);
}

TEST_F(LockboxContentsTest, LoadAndVerifyBadData) {
  SecureBlob nvram_data;
  ASSERT_NO_FATAL_FAILURE(
      GenerateNvramData(NvramVersion::kVersion2, &nvram_data));
  LoadAndVerify(nvram_data, {17}, LockboxError::kHashMismatch);
}
}  // namespace cryptohome
