// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/mount_encrypted/encryption_key.h"

#include <memory>

#include <base/bind.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/logging.h>

#include <vboot/tlcl.h>

#include <gtest/gtest.h>

#include "cryptohome/cryptolib.h"
#include "cryptohome/mount_encrypted/tlcl_stub.h"
#include "cryptohome/mount_encrypted/tpm.h"

namespace {

// Size of the encryption key (256 bit AES) in bytes.
const size_t kEncryptionKeySize = 32;

// This is the constant size of the encrypted form of an encryption key, when
// encrypted with the system key, using 256 bit AES-CBC encryption. Even though
// the size of the key we encrypt is always 32 bytes, we use the standard
// padding scheme for data of variable length, which adds one padding block in
// addition two the initial two blocks containing the key material.
const size_t kWrappedKeySize = 48;

#if USE_TPM2

static const uint32_t kEncStatefulIndex = 0x800005;
static const uint32_t kEncStatefulSize = 40;

const uint32_t kEncStatefulAttributesTpm2 =
    TPMA_NV_AUTHWRITE | TPMA_NV_AUTHREAD | TPMA_NV_WRITEDEFINE |
    TPMA_NV_READ_STCLEAR | TPMA_NV_WRITTEN;

// NVRAM space contents used in tests that exercise behavior with existing
// valid NVRAM space contents. Contains a random system key.
const uint8_t kEncStatefulTpm2Contents[] = {
    0x32, 0x4D, 0x50, 0x54, 0x01, 0x00, 0x00, 0x00, 0xa3, 0xea,
    0xd7, 0x78, 0xa6, 0xb4, 0x74, 0xd7, 0x8f, 0xa1, 0x9a, 0xbd,
    0x04, 0x6a, 0xc5, 0x6c, 0x21, 0xc7, 0x60, 0x1c, 0x45, 0xe3,
    0x06, 0xe2, 0x6a, 0x68, 0x94, 0x96, 0x8b, 0x1a, 0xf3, 0x67,
};

// A random encryption key used in tests that exercise existing keys.
const uint8_t kEncryptionKeyEncStatefulTpm2[] = {
    0x7c, 0xdd, 0x2f, 0xba, 0x4b, 0x6d, 0x28, 0x5b, 0xa0, 0x5a, 0xa5,
    0x84, 0x82, 0x41, 0x02, 0x36, 0x7a, 0x17, 0xc6, 0xe4, 0x78, 0x0e,
    0x86, 0x50, 0x6c, 0x09, 0x50, 0x5c, 0x33, 0x57, 0x19, 0xae,
};

// This is kEncryptionKeyEncStatefulTpm2, encrypted with the system key from
// kEncStatefulTpm2Contents.
const uint8_t kWrappedKeyEncStatefulTpm2[] = {
    0xf4, 0xb0, 0x45, 0xc6, 0x24, 0xf8, 0xcd, 0x92, 0xb4, 0x74, 0x9c, 0x2f,
    0x45, 0x5e, 0x23, 0x8b, 0xba, 0xde, 0x67, 0x3b, 0x10, 0x3f, 0x74, 0xf1,
    0x60, 0x64, 0xa2, 0xca, 0x79, 0xce, 0xed, 0xa7, 0x04, 0xd1, 0xa5, 0x06,
    0x80, 0xc5, 0x84, 0xed, 0x34, 0x93, 0xb1, 0x44, 0xa2, 0x0a, 0x4a, 0x3e,
};

#else  // USE_TPM2

const uint32_t kLockboxAttributesTpm1 = TPM_NV_PER_WRITEDEFINE;

// NVRAM space contents used in tests that exercise behavior with existing
// valid NVRAM space contents. This contains a random "lockbox salt", which
// doubles as the system key for TPM 1.2 devices.
const uint8_t kLockboxV2Contents[] = {
    0x00, 0x00, 0x00, 0x02, 0x00, 0xfa, 0x33, 0x18, 0x5b, 0x57, 0x64, 0x83,
    0x57, 0x9a, 0xaa, 0xab, 0xef, 0x1e, 0x39, 0xa3, 0xa1, 0xb9, 0x94, 0xc5,
    0xc9, 0xa8, 0xd9, 0x32, 0xb4, 0xfb, 0x65, 0xb2, 0x5f, 0xb8, 0x60, 0xb8,
    0xfb, 0x94, 0xf4, 0x77, 0xad, 0x53, 0x46, 0x2e, 0xec, 0x13, 0x4a, 0x95,
    0x4a, 0xb8, 0x12, 0x2a, 0xdd, 0xd8, 0xb0, 0xc9, 0x9d, 0xd0, 0x0c, 0x06,
    0x51, 0x12, 0xcc, 0x72, 0x4d, 0x7c, 0x59, 0xb5, 0xe6,
};

// A random encryption key used in tests that exercise existing keys.
const uint8_t kEncryptionKeyLockboxV2[] = {
    0xfa, 0x33, 0x18, 0x5b, 0x57, 0x64, 0x83, 0x57, 0x9a, 0xaa, 0xab, 0xef,
    0x1e, 0x39, 0xa3, 0xa1, 0xb9, 0x94, 0xc5, 0xc9, 0xa8, 0xd9, 0x32, 0xb4,
    0xfb, 0x65, 0xb2, 0x5f, 0xb8, 0x60, 0xb8, 0xfb,
};

// This is kEncryptionKeyLockboxV2, encrypted with the system key from
// kLockboxV2Contents.
const uint8_t kWrappedKeyLockboxV2[] = {
    0x2e, 0x62, 0x9c, 0x5b, 0x32, 0x2f, 0xb4, 0xa6, 0xba, 0x72, 0xb5, 0xf4,
    0x19, 0x2a, 0xe0, 0xd6, 0xdf, 0x56, 0xf7, 0x64, 0xa0, 0xd6, 0x51, 0xe0,
    0xc1, 0x46, 0x85, 0x80, 0x41, 0xbd, 0x41, 0xab, 0xbf, 0x56, 0x32, 0xaa,
    0xe8, 0x04, 0x5b, 0x69, 0xd4, 0x23, 0x8d, 0x99, 0x84, 0xff, 0x20, 0xc3,
};

// A random encryption key used in tests that exercise the situation where NVRAM
// space is missing and we fall back to writing the encryption key to disk.
const uint8_t kEncryptionKeyNeedsFinalization[] = {
    0xa4, 0x46, 0x75, 0x14, 0x38, 0x66, 0x83, 0x14, 0x2f, 0x88, 0x03, 0x31,
    0x0c, 0x13, 0x47, 0x6a, 0x52, 0x78, 0xcd, 0xff, 0xb9, 0x9c, 0x99, 0x9e,
    0x30, 0x0b, 0x79, 0xf7, 0xad, 0x34, 0x2f, 0xb0,
};

// This is kEncryptionKeyNeedsFinalization, obfuscated by encrypting it with a
// well-known hard-coded system key (the SHA-256 hash of "needs finalization").
const uint8_t kWrappedKeyNeedsFinalization[] = {
    0x38, 0x38, 0x9e, 0x59, 0x39, 0x88, 0xae, 0xb8, 0x74, 0xe8, 0x14, 0x58,
    0x78, 0x12, 0x1b, 0xb1, 0xf4, 0x70, 0xb9, 0x0f, 0x76, 0x22, 0x97, 0xe6,
    0x43, 0x21, 0x59, 0x0f, 0x36, 0x86, 0x90, 0x74, 0x23, 0x7f, 0x14, 0xd1,
    0x3d, 0xef, 0x01, 0x92, 0x9c, 0x89, 0x15, 0x85, 0xc5, 0xe5, 0x78, 0x10,
};

#endif  // !USE_TPM2

}  // namespace

class EncryptionKeyTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(tmpdir_.CreateUniqueTempDir());
    ASSERT_TRUE(base::CreateDirectory(
        tmpdir_.GetPath().AppendASCII("mnt/stateful_partition")));
    key_ = std::make_unique<EncryptionKey>(tmpdir_.GetPath());
  }

  void SetOwned() {
    tlcl_.SetOwned({ 0x5e, 0xc2, 0xe7 });
  }

  void SetupSpace(uint32_t index,
                  uint32_t attributes,
                  const uint8_t* data,
                  size_t size) {
    TlclStub::NvramSpaceData* space = tlcl_.GetSpace(index);
    space->contents.assign(data, data + size);
    space->attributes = attributes;
  }

  void WriteWrappedKey(const base::FilePath& path, const uint8_t* key) {
    ASSERT_TRUE(base::CreateDirectory(path.DirName()));
    ASSERT_TRUE(base::WriteFile(path, reinterpret_cast<const char*>(key),
                                kWrappedKeySize));
  }

  void ExpectNeedsFinalization() {
    key_->Persist();
    EXPECT_FALSE(key_->did_finalize());
    EXPECT_TRUE(base::PathExists(key_->needs_finalization_path()));
    EXPECT_FALSE(base::PathExists(key_->key_path()));
  }

  void ExpectFinalized(bool did_finalize_expectation) {
    key_->Persist();
    EXPECT_EQ(did_finalize_expectation, key_->did_finalize());
    EXPECT_FALSE(base::PathExists(key_->needs_finalization_path()));
    EXPECT_TRUE(base::PathExists(key_->key_path()));
  }

  void ExpectFreshKey() {
    key_->LoadChromeOSSystemKey();
    key_->LoadEncryptionKey();
    EXPECT_EQ(key_->encryption_key().size(), kEncryptionKeySize);
    EXPECT_TRUE(key_->is_fresh());
  }

  void ExpectExistingKey(const uint8_t* expected_key) {
    key_->LoadChromeOSSystemKey();
    key_->LoadEncryptionKey();
    EXPECT_EQ(key_->encryption_key().size(), kEncryptionKeySize);
    if (expected_key) {
      EXPECT_EQ(
          std::vector<uint8_t>(expected_key, expected_key + kEncryptionKeySize),
          key_->encryption_key());
    }
    EXPECT_FALSE(key_->is_fresh());
  }

  void CheckSpace(uint32_t index, uint32_t attributes, uint32_t size) {
    TlclStub::NvramSpaceData* space = tlcl_.GetSpace(index);
    EXPECT_EQ(attributes, space->attributes);
    EXPECT_EQ(size, space->contents.size());
    EXPECT_TRUE(space->read_locked);
    EXPECT_TRUE(space->write_locked);
  }

  base::ScopedTempDir tmpdir_;
  TlclStub tlcl_;

  std::unique_ptr<EncryptionKey> key_;
};

#if USE_TPM2

TEST_F(EncryptionKeyTest, TpmClearNoSpaces) {
  ExpectFreshKey();
  EXPECT_FALSE(key_->is_migration_allowed());
  ExpectFinalized(true);
  CheckSpace(kEncStatefulIndex, kEncStatefulAttributesTpm2, kEncStatefulSize);
}

TEST_F(EncryptionKeyTest, TpmOwnedNoSpaces) {
  SetOwned();

  ExpectFreshKey();
  EXPECT_FALSE(key_->is_migration_allowed());
  ExpectNeedsFinalization();
}

TEST_F(EncryptionKeyTest, TpmExistingSpaceNoKeyFile) {
  SetupSpace(kEncStatefulIndex, kEncStatefulAttributesTpm2,
             kEncStatefulTpm2Contents, sizeof(kEncStatefulTpm2Contents));

  ExpectFreshKey();
  EXPECT_FALSE(key_->is_migration_allowed());
  ExpectFinalized(true);
  CheckSpace(kEncStatefulIndex, kEncStatefulAttributesTpm2, kEncStatefulSize);
}

TEST_F(EncryptionKeyTest, TpmExistingSpaceBadKey) {
  SetupSpace(kEncStatefulIndex, kEncStatefulAttributesTpm2,
             kEncStatefulTpm2Contents, sizeof(kEncStatefulTpm2Contents));
  std::vector<uint8_t> wrapped_key(sizeof(kWrappedKeyEncStatefulTpm2), 0xa5);
  WriteWrappedKey(key_->key_path(), wrapped_key.data());

  ExpectFreshKey();
  EXPECT_FALSE(key_->is_migration_allowed());
  ExpectFinalized(true);
  CheckSpace(kEncStatefulIndex, kEncStatefulAttributesTpm2, kEncStatefulSize);
}

TEST_F(EncryptionKeyTest, TpmExistingSpaceBadAttributes) {
  // TODO(crbug.com/840361): See bug description.
  uint32_t attributes = kEncStatefulAttributesTpm2 | TPMA_NV_PLATFORMCREATE;
  SetupSpace(kEncStatefulIndex, attributes,
             kEncStatefulTpm2Contents, sizeof(kEncStatefulTpm2Contents));

  ExpectFreshKey();
  EXPECT_FALSE(key_->is_migration_allowed());
  ExpectFinalized(true);
  CheckSpace(kEncStatefulIndex, attributes, kEncStatefulSize);
}

TEST_F(EncryptionKeyTest, TpmExistingSpaceNotYetWritten) {
  SetupSpace(kEncStatefulIndex, kEncStatefulAttributesTpm2 & ~TPMA_NV_WRITTEN,
             kEncStatefulTpm2Contents, sizeof(kEncStatefulTpm2Contents));

  ExpectFreshKey();
  EXPECT_FALSE(key_->is_migration_allowed());
  ExpectNeedsFinalization();
}

TEST_F(EncryptionKeyTest, TpmExistingSpaceBadContents) {
  std::vector<uint8_t> bad_contents(sizeof(kEncStatefulTpm2Contents), 0xa5);
  SetupSpace(kEncStatefulIndex, kEncStatefulAttributesTpm2,
             bad_contents.data(), bad_contents.size());

  ExpectFreshKey();
  EXPECT_FALSE(key_->is_migration_allowed());
  ExpectFinalized(true);
  CheckSpace(kEncStatefulIndex, kEncStatefulAttributesTpm2, kEncStatefulSize);
}

TEST_F(EncryptionKeyTest, TpmExistingSpaceValid) {
  SetupSpace(kEncStatefulIndex, kEncStatefulAttributesTpm2,
             kEncStatefulTpm2Contents, sizeof(kEncStatefulTpm2Contents));
  WriteWrappedKey(key_->key_path(), kWrappedKeyEncStatefulTpm2);

  ExpectExistingKey(kEncryptionKeyEncStatefulTpm2);
  EXPECT_FALSE(key_->is_migration_allowed());
  ExpectFinalized(false);
  CheckSpace(kEncStatefulIndex, kEncStatefulAttributesTpm2, kEncStatefulSize);
}

#else  // USE_TPM2

TEST_F(EncryptionKeyTest, TpmClearNoSpaces) {
  ExpectFreshKey();
  EXPECT_TRUE(key_->is_migration_allowed());
  ExpectNeedsFinalization();
}

TEST_F(EncryptionKeyTest, TpmOwnedNoSpaces) {
  SetOwned();

  ExpectFreshKey();
  EXPECT_TRUE(key_->is_migration_allowed());
  ExpectNeedsFinalization();
}

TEST_F(EncryptionKeyTest, TpmClearExistingLockboxV2Unowned) {
  SetupSpace(kLockboxIndex, kLockboxAttributesTpm1, kLockboxV2Contents,
             sizeof(kLockboxV2Contents));

  ExpectFreshKey();
  EXPECT_TRUE(key_->is_migration_allowed());
  ExpectNeedsFinalization();
}

TEST_F(EncryptionKeyTest, TpmOwnedExistingLockboxV2Finalize) {
  SetupSpace(kLockboxIndex, kLockboxAttributesTpm1, kLockboxV2Contents,
             sizeof(kLockboxV2Contents));
  SetOwned();

  ExpectFreshKey();
  EXPECT_FALSE(key_->is_migration_allowed());
  ExpectFinalized(true);
}

TEST_F(EncryptionKeyTest, TpmOwnedExistingLockboxV2Finalized) {
  SetupSpace(kLockboxIndex, kLockboxAttributesTpm1, kLockboxV2Contents,
             sizeof(kLockboxV2Contents));
  SetOwned();
  WriteWrappedKey(key_->key_path(), kWrappedKeyLockboxV2);

  ExpectExistingKey(kEncryptionKeyLockboxV2);
  EXPECT_FALSE(key_->is_migration_allowed());
  ExpectFinalized(false);
}

TEST_F(EncryptionKeyTest, TpmOwnedExistingLockboxV2BadDecrypt) {
  SetupSpace(kLockboxIndex, kLockboxAttributesTpm1, kLockboxV2Contents,
             sizeof(kLockboxV2Contents));
  SetOwned();
  std::vector<uint8_t> wrapped_key(sizeof(kWrappedKeyLockboxV2), 0xa5);
  WriteWrappedKey(key_->key_path(), wrapped_key.data());

  ExpectFreshKey();
  EXPECT_FALSE(key_->is_migration_allowed());
  ExpectFinalized(true);
}

TEST_F(EncryptionKeyTest, TpmClearNeedsFinalization) {
  WriteWrappedKey(key_->needs_finalization_path(),
                  kWrappedKeyNeedsFinalization);

  ExpectExistingKey(kEncryptionKeyNeedsFinalization);
  EXPECT_TRUE(key_->is_migration_allowed());
  ExpectNeedsFinalization();
}

TEST_F(EncryptionKeyTest, TpmOwnedNeedsFinalization) {
  SetOwned();
  WriteWrappedKey(key_->needs_finalization_path(),
                  kWrappedKeyNeedsFinalization);

  ExpectExistingKey(kEncryptionKeyNeedsFinalization);
  EXPECT_TRUE(key_->is_migration_allowed());
  ExpectNeedsFinalization();
}

#endif  // !USE_TPM2
