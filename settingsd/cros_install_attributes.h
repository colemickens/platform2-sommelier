// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SETTINGSD_CROS_INSTALL_ATTRIBUTES_H_
#define SETTINGSD_CROS_INSTALL_ATTRIBUTES_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include <base/macros.h>

#include "settingsd/blob_ref.h"
#include "settingsd/locked_settings.h"
#include "settingsd/settings_document.h"
#include "settingsd/source_delegate.h"

namespace settingsd {

class NVRam;

// A LockedSettingsContainer implementation for the protobuf-encoded install
// attributes format used by Chrome OS. There is actually no signature or MAC on
// this container, as its SHA256-hash is checked against NVRAM contents.
class CrosInstallAttributesContainer : public LockedSettingsContainer {
 public:
  ~CrosInstallAttributesContainer() override;

  // LockedSettingsContainer:
  BlobRef GetData() const override;

  // A function suitable to use as a SettingsBlobParserFunction.
  static std::unique_ptr<LockedSettingsContainer> Parse(
      const std::string& format,
      BlobRef data);

 private:
  explicit CrosInstallAttributesContainer(std::vector<uint8_t> data);

  // LockedSettingsContainer:
  std::unique_ptr<SettingsDocument> DecodePayloadInternal() override;

  const std::vector<uint8_t> data_;

  DISALLOW_COPY_AND_ASSIGN(CrosInstallAttributesContainer);
};

// A source delegate that verifies install attributes containers against
// verification data (size and hash) stored in NVRAM by Chrome OS.
//
// TODO(mnissler): Handle the write path via InsertBlob() too? The code could
// just write the NVRAM and lock it as part of ValidateContainer.
class CrosInstallAttributesSourceDelegate : public SourceDelegate {
 public:
  static const uint32_t kReservedSizeBytes = sizeof(uint32_t);
  static const uint32_t kReservedFlagsBytes = sizeof(uint8_t);
  static const uint32_t kReservedSaltBytesV1 = 7;
  static const uint32_t kReservedSaltBytesV2 = 32;
  static const uint32_t kReservedDigestBytes = 32;
  static const uint32_t kReservedNvramBytesV1 =
      kReservedSizeBytes + kReservedFlagsBytes + kReservedSaltBytesV1 +
      kReservedDigestBytes;
  static const uint32_t kReservedNvramBytesV2 =
      kReservedSizeBytes + kReservedFlagsBytes + kReservedSaltBytesV2 +
      kReservedDigestBytes;

  explicit CrosInstallAttributesSourceDelegate(const NVRam* nvram,
                                               uint32_t nvram_index);
  ~CrosInstallAttributesSourceDelegate() override;

  // SourceDelegate:
  bool ValidateVersionComponent(
      const LockedVersionComponent& component) const override;
  bool ValidateContainer(
      const LockedSettingsContainer& container) const override;

  // A function suitable for use as a SourceDelegateFactory function after
  // binding |nvram| as a parameter. Note that the function extracts the NVRAM
  // index to use from |settings|.
  static std::unique_ptr<SourceDelegate> Create(
      const NVRam* nvram,
      const std::string& source_id,
      const SettingsService& settings);

 private:
  // Extract verification parameters from |nvram_|.
  bool ExtractNVRamParameters(size_t* size,
                              std::vector<uint8_t>* salt,
                              std::vector<uint8_t>* hash) const;

  // The NVRam to validate against.
  const NVRam* nvram_;

  // The index of the NVRam space that holds the verification data.
  const uint32_t nvram_index_;

  DISALLOW_COPY_AND_ASSIGN(CrosInstallAttributesSourceDelegate);
};

}  // namespace settingsd

#endif  // SETTINGSD_CROS_INSTALL_ATTRIBUTES_H_
