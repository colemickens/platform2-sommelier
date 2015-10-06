// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fides/cros_install_attributes.h"

#include <arpa/inet.h>
#include <stddef.h>

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <base/logging.h>

#include "bindings/install_attributes.pb.h"

#include "fides/crypto.h"
#include "fides/identifier_utils.h"
#include "fides/key.h"
#include "fides/nvram.h"
#include "fides/settings_keys.h"
#include "fides/settings_service.h"
#include "fides/source.h"
#include "fides/version_stamp.h"

namespace fides {

namespace {

// A SettingsDocument class that wraps an install attributes protobuf message
// and exports the key-value pairs stored in install attributes via the
// SettingsDocument interface.
class CrosInstallAttributesDocument : public SettingsDocument {
 public:
  explicit CrosInstallAttributesDocument(
      std::unique_ptr<cryptohome::SerializedInstallAttributes>
          install_attributes_message);
  ~CrosInstallAttributesDocument() override;

  // SettingsDocument:
  BlobRef GetValue(const Key& key) const override;
  std::set<Key> GetKeys(const Key& prefix) const override;
  std::set<Key> GetDeletions(const Key& prefix) const override;
  VersionStamp GetVersionStamp() const override;
  bool HasKeysOrDeletions(const Key& prefix) const override;

 private:
  // Sanitizes an attribute name and converts it to a Key.
  static bool SanitizeKey(const std::string& attribute_name, Key* key);

  std::unique_ptr<cryptohome::SerializedInstallAttributes>
      install_attributes_message_;

  DISALLOW_COPY_AND_ASSIGN(CrosInstallAttributesDocument);
};

CrosInstallAttributesDocument::CrosInstallAttributesDocument(
    std::unique_ptr<cryptohome::SerializedInstallAttributes>
        install_attributes_message)
    : install_attributes_message_(std::move(install_attributes_message)) {}

CrosInstallAttributesDocument::~CrosInstallAttributesDocument() {}

BlobRef CrosInstallAttributesDocument::GetValue(const Key& key) const {
  for (auto& attribute : *install_attributes_message_->mutable_attributes()) {
    Key attribute_key;
    if (SanitizeKey(attribute.name(), &attribute_key) && attribute_key == key)
      return BlobRef(attribute.mutable_value());
  }

  return BlobRef();
}

std::set<Key> CrosInstallAttributesDocument::GetKeys(const Key& prefix) const {
  std::set<Key> result;
  for (auto& attribute : install_attributes_message_->attributes()) {
    Key attribute_key;
    if (SanitizeKey(attribute.name(), &attribute_key))
      result.insert(attribute_key);
  }

  return result;
}

std::set<Key> CrosInstallAttributesDocument::GetDeletions(
    const Key& prefix) const {
  // Install attributes never contain deletions.
  return std::set<Key>();
}

VersionStamp CrosInstallAttributesDocument::GetVersionStamp() const {
  // Install attributes never contain versions. This means they can never
  // supersede values received from other sources.
  return VersionStamp();
}

bool CrosInstallAttributesDocument::HasKeysOrDeletions(
    const Key& prefix) const {
  return !GetKeys(prefix).empty();
}

// static
bool CrosInstallAttributesDocument::SanitizeKey(
    const std::string& attribute_name,
    Key* key) {
  CHECK(key);

  // Existing code has a bug to terminate attribute names with a NULL character,
  // which gets stripped by initializing with the c_str() pointer.
  std::string sanitized_name(attribute_name.c_str());
  if (!Key::IsValidKey(sanitized_name))
    return false;

  *key = Key(sanitized_name);
  return true;
}

}  // namespace

CrosInstallAttributesContainer::~CrosInstallAttributesContainer() {}

BlobRef CrosInstallAttributesContainer::GetData() const {
  return BlobRef(&data_);
}

CrosInstallAttributesContainer::CrosInstallAttributesContainer(
    std::vector<uint8_t> data)
    : data_(std::move(data)) {}

// static
std::unique_ptr<LockedSettingsContainer> CrosInstallAttributesContainer::Parse(
    const std::string& format,
    BlobRef data) {
  // TODO(mnissler): Avoid the copy here.
  return std::unique_ptr<LockedSettingsContainer>(
      new CrosInstallAttributesContainer(data.ToVector()));
}

std::unique_ptr<SettingsDocument>
CrosInstallAttributesContainer::DecodePayloadInternal() {
  BlobRef blob(&data_);
  std::unique_ptr<cryptohome::SerializedInstallAttributes>
      install_attributes_message(new cryptohome::SerializedInstallAttributes);
  if (install_attributes_message->ParseFromArray(blob.data(), blob.size())) {
    return std::unique_ptr<SettingsDocument>(new CrosInstallAttributesDocument(
        std::move(install_attributes_message)));
  }

  return nullptr;
}

const uint32_t CrosInstallAttributesSourceDelegate::kReservedSizeBytes;
const uint32_t CrosInstallAttributesSourceDelegate::kReservedFlagsBytes;
const uint32_t CrosInstallAttributesSourceDelegate::kReservedSaltBytesV1;
const uint32_t CrosInstallAttributesSourceDelegate::kReservedSaltBytesV2;
const uint32_t CrosInstallAttributesSourceDelegate::kReservedDigestBytes;
const uint32_t CrosInstallAttributesSourceDelegate::kReservedNvramBytesV1;
const uint32_t CrosInstallAttributesSourceDelegate::kReservedNvramBytesV2;

CrosInstallAttributesSourceDelegate::CrosInstallAttributesSourceDelegate(
    const NVRam* nvram,
    uint32_t nvram_index)
    : nvram_(nvram), nvram_index_(nvram_index) {}

CrosInstallAttributesSourceDelegate::~CrosInstallAttributesSourceDelegate() {}

bool CrosInstallAttributesSourceDelegate::ValidateVersionComponent(
    const LockedVersionComponent& component) const {
  return false;
}

bool CrosInstallAttributesSourceDelegate::ValidateContainer(
    const LockedSettingsContainer& container) const {
  // Extract the verification parameters from |nvram_|.
  size_t size;
  std::vector<uint8_t> salt;
  std::vector<uint8_t> hash;
  if (!ExtractNVRamParameters(&size, &salt, &hash))
    return false;

  // Verify size.
  BlobRef container_data = container.GetData();
  if (size != container_data.size()) {
    LOG(WARNING) << "Blob size doesn't match NVRAM: " << container_data.size()
                 << " vs " << size;
    return false;
  }

  // Verify the hash.
  std::vector<uint8_t> salted_container_data(container_data.ToVector());
  salted_container_data.insert(salted_container_data.end(), salt.begin(),
                               salt.end());
  if (!crypto::VerifyDigest(crypto::kDigestSha256,
                            BlobRef(&salted_container_data), BlobRef(&hash))) {
    LOG(WARNING) << "Blob digest doesn't match NVRAM.";
    return false;
  }

  return true;
}

// static
std::unique_ptr<SourceDelegate> CrosInstallAttributesSourceDelegate::Create(
    const NVRam* nvram,
    const std::string& source_id,
    const SettingsService& settings) {
  // Extract the index from the source configuration.
  BlobRef nvram_index_value = settings.GetValue(
      MakeSourceKey(source_id).Extend({keys::sources::kNVRamIndex}));
  if (!nvram_index_value.valid())
    return nullptr;

  size_t pos = 0;
  uint32_t nvram_index = std::stoul(nvram_index_value.ToString(), &pos, 0);
  if (pos == 0 || pos != nvram_index_value.size())
    return nullptr;

  return std::unique_ptr<SourceDelegate>(
      new CrosInstallAttributesSourceDelegate(nvram, nvram_index));
}

bool CrosInstallAttributesSourceDelegate::ExtractNVRamParameters(
    size_t* size,
    std::vector<uint8_t>* salt,
    std::vector<uint8_t>* hash) const {
  // A locked NVRam is required.
  bool read_lock = false;
  bool write_lock = false;
  if (nvram_->IsSpaceLocked(nvram_index_, &read_lock, &write_lock) !=
          NVRam::Status::kSuccess ||
      !write_lock) {
    LOG(WARNING) << "NVRam space " << nvram_index_ << " not locked.";
    return false;
  }

  std::vector<uint8_t> nvram_data;
  if (nvram_->ReadSpace(nvram_index_, &nvram_data) != NVRam::Status::kSuccess) {
    LOG(ERROR) << "Failed to read NVRam space " << nvram_index_;
    return false;
  }

  // If the read is successful, but the size is not an expected value,
  // we've got tampering or an unexpected bug/race during set.
  switch (nvram_data.size()) {
  case kReservedNvramBytesV1:
    salt->resize(kReservedSaltBytesV1);
    break;
  case kReservedNvramBytesV2:
    salt->resize(kReservedSaltBytesV2);
    break;
  default:
    LOG(ERROR) << "Unexpected NVRAM size: " << nvram_data.size();
    return false;
  }

  const uint8_t* nvram_ptr = nvram_data.data();

  // Extract the expected data size.
  //
  // For reasons lost to history, the size field is stored in inverse (!) host
  // byte order. The code below originates from cryptohome. Note that the loop
  // reads the size field in little-endian format in an endian-independent way.
  // The ntohl invocation then inverts byte order to big-endian on little-endian
  // hosts.
  uint32_t stored_size = 0;
  for (uint32_t index = 0; index < kReservedSizeBytes; ++index)
    stored_size |= static_cast<uint32_t>(nvram_ptr[index]) << (index * 8);
  *size = ntohl(stored_size);
  nvram_ptr += kReservedSizeBytes;

  // Strip the flags byte, which is currently unused.
  nvram_ptr += kReservedFlagsBytes;

  // Grab the salt.
  std::copy(nvram_ptr, nvram_ptr + salt->size(), salt->begin());
  nvram_ptr += salt->size();

  // Grab the hash.
  hash->resize(kReservedDigestBytes);
  std::copy(nvram_ptr, nvram_ptr + hash->size(), hash->begin());
  nvram_ptr += hash->size();

  // Sanity check for making sure the pointer arithmetic above is correct.
  CHECK_EQ(nvram_ptr, nvram_data.data() + nvram_data.size());

  return true;
}

}  // namespace fides
