// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for TpmInit.

#include "cryptohome/tpm_init.h"

#include <map>
#include <string>

#include <base/files/file_path.h>
#include <brillo/secure_blob.h>

#include "cryptohome/mock_platform.h"
#include "cryptohome/mock_tpm.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;

namespace cryptohome {

extern const base::FilePath kMiscTpmCheckEnabledFile;
extern const base::FilePath kMiscTpmCheckOwnedFile;
extern const base::FilePath kTpmTpmCheckEnabledFile;
extern const base::FilePath kTpmTpmCheckOwnedFile;
extern const base::FilePath kTpmOwnedFileOld;
extern const base::FilePath kTpmStatusFileOld;
extern const base::FilePath kTpmOwnedFile;
extern const base::FilePath kTpmStatusFile;
extern const base::FilePath kOpenCryptokiPath;
extern const base::FilePath kDefaultCryptohomeKeyFile;

// Tests that need to do more setup work before calling Service::Initialize can
// use this instead of ServiceTest.
class TpmInitTest : public ::testing::Test {
 public:
  TpmInitTest()
    : is_tpm_owned_(false),
      is_tpm_being_owned_(false),
      is_tpm_initialized_(false),
      tpm_init_(&tpm_, &platform_) {}
  ~TpmInitTest() override = default;

  // Default mock implementations for |tpm_| methods.
  // For TPM-related flags: enabled is always true, other flags are settable.
  bool IsTpmOwned() const { return is_tpm_owned_; }
  void SetIsTpmOwned(bool is_tpm_owned) {
    is_tpm_owned_ = is_tpm_owned;
  }
  bool IsTpmBeingOwned() const { return is_tpm_being_owned_; }
  void SetIsTpmBeingOwned(bool is_tpm_being_owned) {
    is_tpm_being_owned_ = is_tpm_being_owned;
  }
  bool IsTpmInitialized() { return is_tpm_initialized_; }
  void SetIsTpmInitialized(bool is_tpm_initialized) {
    is_tpm_initialized_ = is_tpm_initialized;
  }
  bool PerformTpmEnabledOwnedCheck(bool* is_enabled, bool* is_owned) {
    *is_enabled = true;
    *is_owned = is_tpm_owned_;
    return true;
  }
  bool GetRandomData(size_t length, brillo::Blob* data) const {
    data->resize(length, 0);
    return true;
  }

  // Default mock implementations for |platform_| methods.
  // Files are emulated using |files_| map: <file path> -> <file contents>.
  bool FileExists(const base::FilePath& path) const {
    return files_.count(path) > 0;
  }
  bool FileMove(const base::FilePath& from, const base::FilePath& to) {
    if (!FileExists(from)) {
      return false;
    }
    if (FileExists(to)) {
      return false;
    }
    files_[to] = files_[from];
    return FileDelete(from, false);
  }
  bool FileDelete(const base::FilePath& path, bool /* recursive */) {
    return files_.erase(path) == 1;
  }
  bool FileTouch(const base::FilePath& path) {
    files_.emplace(path, brillo::Blob());
    return FileExists(path);
  }
  bool GetFileSize(const base::FilePath& path, int64_t* size) {
    if (!FileExists(path)) {
      return false;
    }
    *size = files_[path].size();
    return true;
  }
  bool FileRead(const base::FilePath& path, brillo::Blob* blob) {
    if (!FileExists(path)) {
      return false;
    }
    *blob = files_[path];
    return true;
  }
  bool FileReadToString(const base::FilePath& path, std::string* str) {
    brillo::Blob blob;
    if (!FileRead(path, &blob)) {
      return false;
    }
    str->assign(reinterpret_cast<char*>(blob.data()), blob.size());
    return true;
  }
  bool FileWrite(const base::FilePath& path, const brillo::Blob& blob) {
    files_[path] = blob;
    return true;
  }
  bool FileWriteAtomic(const base::FilePath& path,
                       const brillo::Blob& blob,
                       mode_t /* mode */) {
    return FileWrite(path, blob);
  }

  void SetUp() override {
    ON_CALL(tpm_, IsEnabled())
      .WillByDefault(Return(true));
    ON_CALL(tpm_, IsOwned())
      .WillByDefault(Invoke(this, &TpmInitTest::IsTpmOwned));
    ON_CALL(tpm_, SetIsOwned(_))
      .WillByDefault(Invoke(this, &TpmInitTest::SetIsTpmOwned));
    ON_CALL(tpm_, IsBeingOwned())
      .WillByDefault(Invoke(this, &TpmInitTest::IsTpmBeingOwned));
    ON_CALL(tpm_, SetIsBeingOwned(_))
      .WillByDefault(Invoke(this, &TpmInitTest::SetIsTpmBeingOwned));
    ON_CALL(tpm_, IsInitialized())
      .WillByDefault(Invoke(this, &TpmInitTest::IsTpmInitialized));
    ON_CALL(tpm_, SetIsInitialized(_))
      .WillByDefault(Invoke(this, &TpmInitTest::SetIsTpmInitialized));
    ON_CALL(tpm_, PerformEnabledOwnedCheck(_, _))
      .WillByDefault(Invoke(this, &TpmInitTest::PerformTpmEnabledOwnedCheck));

    ON_CALL(tpm_, GetRandomData(_, _))
      .WillByDefault(Invoke(this, &TpmInitTest::GetRandomData));

    ON_CALL(platform_, FileExists(_))
      .WillByDefault(Invoke(this, &TpmInitTest::FileExists));
    ON_CALL(platform_, Move(_, _))
      .WillByDefault(Invoke(this, &TpmInitTest::FileMove));
    ON_CALL(platform_, DeleteFile(_, _))
      .WillByDefault(Invoke(this, &TpmInitTest::FileDelete));
    ON_CALL(platform_, DeleteFileDurable(_, _))
      .WillByDefault(Invoke(this, &TpmInitTest::FileDelete));
    ON_CALL(platform_, TouchFileDurable(_))
      .WillByDefault(Invoke(this, &TpmInitTest::FileTouch));
    ON_CALL(platform_, GetFileSize(_, _))
      .WillByDefault(Invoke(this, &TpmInitTest::GetFileSize));
    ON_CALL(platform_, ReadFile(_, _))
      .WillByDefault(Invoke(this, &TpmInitTest::FileRead));
    ON_CALL(platform_, WriteFile(_, _))
      .WillByDefault(Invoke(this, &TpmInitTest::FileWrite));
    ON_CALL(platform_, WriteFileAtomic(_, _, _))
      .WillByDefault(Invoke(this, &TpmInitTest::FileWriteAtomic));
    ON_CALL(platform_, WriteFileAtomicDurable(_, _, _))
      .WillByDefault(Invoke(this, &TpmInitTest::FileWriteAtomic));
    ON_CALL(platform_, DataSyncFile(_))
      .WillByDefault(Return(true));
  }

  void TearDown() override {}

 protected:
  bool is_tpm_owned_;
  bool is_tpm_being_owned_;
  bool is_tpm_initialized_;
  std::map<base::FilePath, brillo::Blob> files_;
  NiceMock<MockTpm> tpm_;
  NiceMock<MockPlatform> platform_;

  // Declare tpm_init_ last, so it gets destroyed before all the mocks.
  TpmInit tpm_init_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TpmInitTest);
};

TEST_F(TpmInitTest, AlreadyOwnedSuccess) {
  bool took_ownership = false;
  SetIsTpmOwned(true);
  FileTouch(kTpmOwnedFile);
  ASSERT_TRUE(tpm_init_.InitializeTpm(&took_ownership));
  ASSERT_FALSE(took_ownership);
}

TEST_F(TpmInitTest, TakeOwnershipSuccess) {
  // Setup TPM.
  EXPECT_CALL(tpm_, SetIsOwned(false))
    .Times(1);
  EXPECT_CALL(tpm_, SetIsEnabled(true))
    .Times(1);
  EXPECT_TRUE(tpm_init_.SetupTpm(false));
  EXPECT_TRUE(IsTpmInitialized());
  EXPECT_FALSE(IsTpmOwned());
  EXPECT_FALSE(FileExists(kTpmOwnedFile));
  ::testing::Mock::VerifyAndClearExpectations(&tpm_);

  // Take Ownership.
  EXPECT_CALL(tpm_, IsEndorsementKeyAvailable())
    .WillOnce(Return(false))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(tpm_, CreateEndorsementKey())
    .WillOnce(Return(true));
  EXPECT_CALL(tpm_, TakeOwnership(_, _))
    .WillOnce(Return(true));
  EXPECT_CALL(tpm_, TestTpmAuth(_))
    .WillOnce(Return(true));
  EXPECT_CALL(tpm_, InitializeSrk(_))
    .WillOnce(Return(true));
  EXPECT_CALL(tpm_, ChangeOwnerPassword(_, _))
    .WillOnce(Return(true));
  EXPECT_CALL(tpm_, SetOwnerPassword(_))
    .Times(1);
  bool took_ownership = false;
  EXPECT_TRUE(tpm_init_.InitializeTpm(&took_ownership));
  EXPECT_TRUE(took_ownership);
  EXPECT_TRUE(IsTpmOwned());
  EXPECT_FALSE(IsTpmBeingOwned());
  EXPECT_TRUE(FileExists(kTpmOwnedFile));
}

TEST_F(TpmInitTest, ContinueInterruptedInitializeSrk) {
  // Setup TPM.
  EXPECT_CALL(tpm_, SetIsOwned(false))
    .Times(1);
  EXPECT_CALL(tpm_, SetIsEnabled(true))
    .Times(1);
  EXPECT_TRUE(tpm_init_.SetupTpm(false));
  EXPECT_TRUE(IsTpmInitialized());
  EXPECT_FALSE(IsTpmOwned());
  EXPECT_FALSE(FileExists(kTpmOwnedFile));
  ::testing::Mock::VerifyAndClearExpectations(&tpm_);

  // SRK is not initialized during the first attempt.
  EXPECT_CALL(tpm_, IsEndorsementKeyAvailable())
    .WillOnce(Return(false))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(tpm_, CreateEndorsementKey())
    .WillOnce(Return(true));
  EXPECT_CALL(tpm_, TakeOwnership(_, _))
    .WillOnce(Return(true));
  EXPECT_CALL(tpm_, TestTpmAuth(_))
    .WillOnce(Return(true));
  EXPECT_CALL(tpm_, InitializeSrk(_))
    .WillOnce(Return(false));
  bool took_ownership = false;
  EXPECT_FALSE(tpm_init_.InitializeTpm(&took_ownership));
  EXPECT_TRUE(took_ownership);
  EXPECT_TRUE(IsTpmOwned());
  EXPECT_FALSE(IsTpmBeingOwned());
  EXPECT_FALSE(FileExists(kTpmOwnedFile));
  ::testing::Mock::VerifyAndClearExpectations(&tpm_);

  // Attempt 2: repeat SetupTpm and InitializeTpm.
  SetIsTpmInitialized(false);
  EXPECT_CALL(tpm_, SetIsOwned(true))
    .Times(1);
  EXPECT_CALL(tpm_, SetIsEnabled(true))
    .Times(1);
  EXPECT_TRUE(tpm_init_.SetupTpm(false));
  EXPECT_TRUE(IsTpmInitialized());
  EXPECT_TRUE(IsTpmOwned());
  EXPECT_FALSE(FileExists(kTpmOwnedFile));
  ::testing::Mock::VerifyAndClearExpectations(&tpm_);

  // InitializeTpm() should pick up where it left off. This time initialize SRK.
  EXPECT_CALL(tpm_, IsEndorsementKeyAvailable())
    .Times(0);
  EXPECT_CALL(tpm_, TestTpmAuth(_))
    .WillOnce(Return(true));
  EXPECT_CALL(tpm_, InitializeSrk(_))
    .WillOnce(Return(true));
  EXPECT_CALL(tpm_, ChangeOwnerPassword(_, _))
    .WillOnce(Return(true));
  EXPECT_CALL(tpm_, SetOwnerPassword(_))
    .Times(1);
  took_ownership = false;
  EXPECT_TRUE(tpm_init_.InitializeTpm(&took_ownership));
  EXPECT_FALSE(took_ownership);
  EXPECT_TRUE(IsTpmOwned());
  EXPECT_FALSE(IsTpmBeingOwned());
  EXPECT_TRUE(FileExists(kTpmOwnedFile));
}

}  // namespace cryptohome
