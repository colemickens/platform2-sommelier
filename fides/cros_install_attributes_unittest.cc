// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fides/cros_install_attributes.h"

#include <arpa/inet.h>
#include <stdint.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include <base/files/scoped_temp_dir.h>
#include <base/macros.h>
#include <gtest/gtest.h>

#include "bindings/install_attributes.pb.h"

#include "fides/crypto.h"
#include "fides/mock_nvram.h"
#include "fides/mock_settings_document.h"
#include "fides/settings_document_manager.h"
#include "fides/settings_keys.h"
#include "fides/simple_settings_map.h"
#include "fides/test_helpers.h"

namespace fides {

namespace {

const char kTestSource[] = "test_source";
const char kTestKey[] = "test.foo";
const char kTestValue[] = "test_value";
const uint32_t kTestNVRamIndex = 42;
const uint8_t
    kTestSalt[CrosInstallAttributesSourceDelegate::kReservedSaltBytesV2] =
        "test salt";

}  // namespace

class CrosInstallAttributesTest : public testing::Test {
 public:
  void SetUp() {
    // Prepare a serialized install attributes blob for testing.
    cryptohome::SerializedInstallAttributes install_attributes;
    cryptohome::SerializedInstallAttributes::Attribute* attribute =
        install_attributes.add_attributes();
    attribute->set_name(kTestKey);
    attribute->set_value(kTestValue);
    ASSERT_TRUE(
        install_attributes.SerializeToString(&install_attributes_blob_));

    // Prepare a mock NVRAM.
    MockNVRam::Space* space = nvram_.GetSpace(kTestNVRamIndex);
    space->locked_for_reading_ = true;
    space->locked_for_writing_ = true;
    InitSpace(install_attributes_blob_, install_attributes_blob_.size(),
              &space->data_);

    // Bring up a SettingsDocumentManager to consumer the blob.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    manager_.reset(new SettingsDocumentManager(
        CrosInstallAttributesContainer::Parse,
        std::bind(CrosInstallAttributesSourceDelegate::Create, &nvram_,
                  std::placeholders::_1, std::placeholders::_2),
        temp_dir_.path().value(),
        std::unique_ptr<SettingsMap>(new SimpleSettingsMap()),
        MakeTrustedDocument()));
    manager_->Init();
  }

  static void InitSpace(const std::string& data,
                        uint32_t size,
                        std::vector<uint8_t>* space) {
    // Compute the size field.
    space->resize(CrosInstallAttributesSourceDelegate::kReservedNvramBytesV2);
    size = htonl(size);
    for (size_t i = 0; i < sizeof(size); ++i)
      (*space)[i] = (size >> (8 * i)) & 0xff;

    // Put the dummy salt.
    std::copy(kTestSalt, kTestSalt + sizeof(kTestSalt),
              space->begin() +
                  CrosInstallAttributesSourceDelegate::kReservedSizeBytes +
                  CrosInstallAttributesSourceDelegate::kReservedFlagsBytes);

    // Compute the digest.
    std::vector<uint8_t> salted_blob;
    salted_blob.insert(salted_blob.end(), data.begin(), data.end());
    salted_blob.insert(salted_blob.end(), kTestSalt,
                       kTestSalt + sizeof(kTestSalt));
    std::vector<uint8_t> digest;
    ASSERT_TRUE(crypto::ComputeDigest(crypto::kDigestSha256,
                                      BlobRef(&salted_blob), &digest));
    std::copy(digest.begin(), digest.end(),
              space->begin() +
                  CrosInstallAttributesSourceDelegate::kReservedSizeBytes +
                  CrosInstallAttributesSourceDelegate::kReservedFlagsBytes +
                  CrosInstallAttributesSourceDelegate::kReservedSaltBytesV2);
  }

  static std::unique_ptr<SettingsDocument> MakeTrustedDocument() {
    // Prepare a trusted settings document that defines an install attributes
    // source.
    std::unique_ptr<MockSettingsDocument> trusted_document(
        new MockSettingsDocument(VersionStamp()));
    trusted_document->SetKey(
        MakeSourceKey(kTestSource).Extend({keys::sources::kStatus}),
        SettingStatusToString(kSettingStatusActive));
    trusted_document->SetKey(
        MakeSourceKey(kTestSource).Extend({keys::sources::kNVRamIndex}),
        std::to_string(kTestNVRamIndex));
    trusted_document->SetKey(
        MakeSourceKey(kTestSource)
            .Extend({keys::sources::kAccess})
            .Append(Key(kTestKey)),
        SettingStatusToString(kSettingStatusActive));
    return std::move(trusted_document);
  }

  base::ScopedTempDir temp_dir_;
  std::string install_attributes_blob_;
  MockNVRam nvram_;
  std::unique_ptr<SettingsDocumentManager> manager_;
};

TEST_F(CrosInstallAttributesTest, Success) {
  EXPECT_EQ(
      SettingsDocumentManager::kInsertionStatusSuccess,
      manager_->InsertBlob(kTestSource, BlobRef(&install_attributes_blob_)));

  // Verify that the value has shown up.
  EXPECT_TRUE(BlobRef(kTestValue).Equals(manager_->GetValue(Key(kTestKey))));
}

TEST_F(CrosInstallAttributesTest, UndefinedNVRamSpace) {
  nvram_.DeleteSpace(kTestNVRamIndex);
  EXPECT_EQ(
      SettingsDocumentManager::kInsertionStatusValidationError,
      manager_->InsertBlob(kTestSource, BlobRef(&install_attributes_blob_)));
  EXPECT_FALSE(manager_->GetValue(Key(kTestKey)).valid());
}

TEST_F(CrosInstallAttributesTest, UnlockedNVRamSpace) {
  nvram_.GetSpace(kTestNVRamIndex)->locked_for_writing_ = false;
  EXPECT_EQ(
      SettingsDocumentManager::kInsertionStatusValidationError,
      manager_->InsertBlob(kTestSource, BlobRef(&install_attributes_blob_)));
  EXPECT_FALSE(manager_->GetValue(Key(kTestKey)).valid());
}

TEST_F(CrosInstallAttributesTest, BadSize) {
  InitSpace(install_attributes_blob_, 1,
            &nvram_.GetSpace(kTestNVRamIndex)->data_);
  EXPECT_EQ(
      SettingsDocumentManager::kInsertionStatusValidationError,
      manager_->InsertBlob(kTestSource, BlobRef(&install_attributes_blob_)));
  EXPECT_FALSE(manager_->GetValue(Key(kTestKey)).valid());
}

TEST_F(CrosInstallAttributesTest, BadDigest) {
  install_attributes_blob_.back() ^= 0xff;
  EXPECT_EQ(
      SettingsDocumentManager::kInsertionStatusValidationError,
      manager_->InsertBlob(kTestSource, BlobRef(&install_attributes_blob_)));
  EXPECT_FALSE(manager_->GetValue(Key(kTestKey)).valid());
}

}  // namespace fides
