// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Functional tests for LECredentialManager + SignInHashTree.
#include <utility>

#include <base/files/scoped_temp_dir.h>
#include <brillo/secure_blob.h>
#include <gmock/gmock.h>
#include <gtest/gtest_prod.h>

#include "cryptohome/cryptolib.h"
#include "cryptohome/fake_le_credential_backend.h"
#include "cryptohome/le_credential_manager.h"

namespace {

// All the keys are 32 bytes long.
const brillo::SecureBlob kLeSecret1 = {
    {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x00,
     0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x00, 0x01,
     0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x00, 0x02}};

const brillo::SecureBlob kLeSecret2 = {
    {0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x10,
     0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x10, 0x11,
     0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x10, 0x12}};

const brillo::SecureBlob kHeSecret1 = {
    {0x00, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x00,
     0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x00, 0x06,
     0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10}};

const brillo::SecureBlob kResetSecret1 = {
    {0x00, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x00,
     0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x00, 0x0B,
     0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15}};

}  // namespace

namespace cryptohome {

class LECredentialManagerUnitTest : public testing::Test {
 public:
  LECredentialManagerUnitTest() {
    CHECK(temp_dir_.CreateUniqueTempDir());
    InitLEManager();
  }

  void InitLEManager() {
    base::FilePath cred_manager_dir =
        temp_dir_.GetPath().Append("low_entropy_creds");
    le_mgr_ =
        std::make_unique<LECredentialManager>(&fake_backend_, cred_manager_dir);
  }

  // Helper function to create a credential & then lock it out.
  // NOTE: Parameterize the secrets once you have more than 1
  // of them.
  uint64_t CreateLockedOutCredential() {
    // TODO(pmalani): fill delay schedule with 0 delays for first 4 attempts and
    // hard limit at 5.
    std::map<uint32_t, uint32_t> stub_delay_sched;
    uint64_t label;
    EXPECT_EQ(LE_CRED_SUCCESS,
              le_mgr_->InsertCredential(kLeSecret1, kHeSecret1, kResetSecret1,
                                        stub_delay_sched, &label));
    brillo::SecureBlob he_secret;
    for (int i = 0; i < LE_MAX_INCORRECT_ATTEMPTS; i++) {
      EXPECT_EQ(LE_CRED_ERROR_INVALID_LE_SECRET,
                le_mgr_->CheckCredential(label, kHeSecret1, &he_secret));
    }
    return label;
  }

  void CorruptLeafCache() {
    // Fill the leafcache file with random data.
    int64_t file_size;
    base::FilePath leaf_cache = temp_dir_.GetPath()
                                    .Append("low_entropy_creds")
                                    .Append(kLeafCacheFileName);
    ASSERT_TRUE(base::GetFileSize(leaf_cache, &file_size));
    std::vector<uint8_t> random_data(file_size);
    CryptoLib::GetSecureRandom(random_data.data(), file_size);
    ASSERT_EQ(
        file_size,
        base::WriteFile(leaf_cache, reinterpret_cast<char*>(random_data.data()),
                        file_size));
  }

  base::ScopedTempDir temp_dir_;
  FakeLECredentialBackend fake_backend_;
  std::unique_ptr<LECredentialManager> le_mgr_;
};

// Basic check: Insert 2 labels, then verify we can retrieve them correctly.
// Here, we don't bother with specifying a delay schedule, we just want
// to check whether a simple Insert and Check works.
TEST_F(LECredentialManagerUnitTest, BasicInsertAndCheck) {
  std::map<uint32_t, uint32_t> stub_delay_sched;
  uint64_t label1;
  uint64_t label2;
  ASSERT_EQ(LE_CRED_SUCCESS,
            le_mgr_->InsertCredential(kLeSecret1, kHeSecret1, kResetSecret1,
                                      stub_delay_sched, &label1));
  ASSERT_EQ(LE_CRED_SUCCESS,
            le_mgr_->InsertCredential(kLeSecret2, kHeSecret1, kResetSecret1,
                                      stub_delay_sched, &label2));
  brillo::SecureBlob he_secret;
  EXPECT_EQ(LE_CRED_SUCCESS,
            le_mgr_->CheckCredential(label1, kLeSecret1, &he_secret));
  EXPECT_EQ(he_secret, kHeSecret1);
  EXPECT_EQ(LE_CRED_ERROR_INVALID_LE_SECRET,
            le_mgr_->CheckCredential(label2, kLeSecret1, &he_secret));
  EXPECT_EQ(LE_CRED_SUCCESS,
            le_mgr_->CheckCredential(label2, kLeSecret2, &he_secret));
  EXPECT_EQ(he_secret, kHeSecret1);
}

// Verify invalid secrets and getting locked out due to too many attempts.
// TODO(pmalani): Update this once we have started modelling the delay schedule
// correctly.
TEST_F(LECredentialManagerUnitTest, LockedOutSecret) {
  uint64_t label1 = CreateLockedOutCredential();

  // NOTE: The current fake backend hard codes the number of attempts at 5, so
  // all subsequent checks will return false.
  brillo::SecureBlob he_secret;
  EXPECT_EQ(LE_CRED_ERROR_TOO_MANY_ATTEMPTS,
            le_mgr_->CheckCredential(label1, kLeSecret1, &he_secret));

  // Check once more to ensure that even after an ERROR_TOO_MANY_ATTEMPTS, the
  // right metadata is stored.
  EXPECT_EQ(LE_CRED_ERROR_TOO_MANY_ATTEMPTS,
            le_mgr_->CheckCredential(label1, kLeSecret1, &he_secret));
}

// Insert a label. Then ensure that a CheckCredential on another non-existent
// label fails.
TEST_F(LECredentialManagerUnitTest, InvalidLabelCheck) {
  std::map<uint32_t, uint32_t> stub_delay_sched;
  uint64_t label1;
  ASSERT_EQ(LE_CRED_SUCCESS,
            le_mgr_->InsertCredential(kLeSecret1, kHeSecret1, kResetSecret1,
                                      stub_delay_sched, &label1));
  // First try a badly encoded label.
  uint64_t invalid_label = ~label1;
  brillo::SecureBlob he_secret;
  EXPECT_EQ(LE_CRED_ERROR_INVALID_LABEL,
            le_mgr_->CheckCredential(invalid_label, kLeSecret1, &he_secret));
  // Next check a valid, but absent label.
  invalid_label = label1 ^ 0x1;
  EXPECT_EQ(LE_CRED_ERROR_INVALID_LABEL,
            le_mgr_->CheckCredential(invalid_label, kLeSecret1, &he_secret));
}

// Insert a credential and then remove it.
// Check that a subsequent CheckCredential on that label fails.
TEST_F(LECredentialManagerUnitTest, BasicInsertRemove) {
  uint64_t label1;
  std::map<uint32_t, uint32_t> stub_delay_sched;
  ASSERT_EQ(LE_CRED_SUCCESS,
            le_mgr_->InsertCredential(kLeSecret1, kHeSecret1, kResetSecret1,
                                      stub_delay_sched, &label1));
  ASSERT_EQ(LE_CRED_SUCCESS, le_mgr_->RemoveCredential(label1));
  brillo::SecureBlob he_secret;
  EXPECT_EQ(LE_CRED_ERROR_INVALID_LABEL,
            le_mgr_->CheckCredential(label1, kHeSecret1, &he_secret));
}

// Check that a reset unlocks a locked out credential.
TEST_F(LECredentialManagerUnitTest, ResetSecret) {
  uint64_t label1 = CreateLockedOutCredential();

  // Ensure that even after an ERROR_TOO_MANY_ATTEMPTS, the right metadata
  // is stored.
  brillo::SecureBlob he_secret;
  ASSERT_EQ(LE_CRED_ERROR_TOO_MANY_ATTEMPTS,
            le_mgr_->CheckCredential(label1, kLeSecret1, &he_secret));

  EXPECT_EQ(LE_CRED_SUCCESS, le_mgr_->ResetCredential(label1, kResetSecret1));

  he_secret.clear();
  // Make sure we can Check successfully, post reset.
  EXPECT_EQ(LE_CRED_SUCCESS,
            le_mgr_->CheckCredential(label1, kLeSecret1, &he_secret));
  EXPECT_EQ(he_secret, kHeSecret1);
}

// Check that an invalid reset doesn't unlock a locked credential.
TEST_F(LECredentialManagerUnitTest, ResetSecretNegative) {
  uint64_t label1 = CreateLockedOutCredential();

  // Ensure that even after an ERROR_TOO_MANY_ATTEMPTS, the right metadata
  // is stored.
  brillo::SecureBlob he_secret;
  ASSERT_EQ(LE_CRED_ERROR_TOO_MANY_ATTEMPTS,
            le_mgr_->CheckCredential(label1, kLeSecret1, &he_secret));

  EXPECT_EQ(LE_CRED_ERROR_INVALID_RESET_SECRET,
            le_mgr_->ResetCredential(label1, kLeSecret1));

  // Make sure that Check still fails.
  EXPECT_EQ(LE_CRED_ERROR_TOO_MANY_ATTEMPTS,
            le_mgr_->CheckCredential(label1, kLeSecret1, &he_secret));
}

// Corrupt the hash cache, and see if subsequent LE operations succeed.
// The two cases being tested are removal after corruption, and insertion
// after corruption.
TEST_F(LECredentialManagerUnitTest, InsertRemoveCorruptHashCache) {
  uint64_t label1;
  std::map<uint32_t, uint32_t> stub_delay_sched;
  ASSERT_EQ(LE_CRED_SUCCESS,
            le_mgr_->InsertCredential(kLeSecret1, kHeSecret1, kResetSecret1,
                                      stub_delay_sched, &label1));

  le_mgr_.reset();
  CorruptLeafCache();
  // Now re-initialize the LE Manager.
  InitLEManager();

  // We should be able to regenerate the HashCache.
  EXPECT_EQ(LE_CRED_SUCCESS, le_mgr_->RemoveCredential(label1));

  // Now let's reinsert the same credential.
  ASSERT_EQ(LE_CRED_SUCCESS,
            le_mgr_->InsertCredential(kLeSecret1, kHeSecret1, kResetSecret1,
                                      stub_delay_sched, &label1));

  le_mgr_.reset();
  CorruptLeafCache();
  // Now re-initialize the LE Manager.
  InitLEManager();

  // Let's make sure future operations work.
  uint64_t label2;
  EXPECT_EQ(LE_CRED_SUCCESS,
            le_mgr_->InsertCredential(kLeSecret1, kHeSecret1, kResetSecret1,
                                      stub_delay_sched, &label2));
  brillo::SecureBlob he_secret;
  EXPECT_EQ(LE_CRED_SUCCESS,
            le_mgr_->CheckCredential(label1, kLeSecret1, &he_secret));
  EXPECT_EQ(LE_CRED_SUCCESS, le_mgr_->RemoveCredential(label1));
  EXPECT_EQ(LE_CRED_SUCCESS, le_mgr_->RemoveCredential(label2));
}

}  // namespace cryptohome
