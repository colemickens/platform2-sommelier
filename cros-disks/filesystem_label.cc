// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/filesystem_label.h"

#include <cctype>
#include <cstring>

#include <base/logging.h>

namespace cros_disks {

namespace {

struct LabelParameters {
  const char* filesystem_type;
  const size_t max_label_length;
};

// Forbidden characters in volume label
const char kForbiddenCharacters[] = "*?.,;:/\\|+=<>[]\"'\t";

// Supported file systems and their parameters
const LabelParameters kSupportedLabelParameters[] = {
    {"vfat", 11}, {"exfat", 15}, {"ntfs", 32}};

const LabelParameters* FindLabelParameters(const std::string& filesystem_type) {
  for (const auto& parameters : kSupportedLabelParameters) {
    if (filesystem_type == parameters.filesystem_type) {
      return &parameters;
    }
  }

  return nullptr;
}

}  // namespace

LabelErrorType ValidateVolumeLabel(const std::string& volume_label,
                                   const std::string& filesystem_type) {
  // Check if the file system is supported for renaming
  const LabelParameters* parameters = FindLabelParameters(filesystem_type);
  if (!parameters) {
    LOG(WARNING) << filesystem_type
                 << " filesystem is not supported for labelling";
    return LabelErrorType::kLabelErrorUnsupportedFilesystem;
  }

  // Check if new volume label satisfies file system volume label conditions
  // Volume label length
  if (volume_label.size() > parameters->max_label_length) {
    LOG(WARNING) << "New volume label '" << volume_label << "' exceeds "
                 << "the limit of '" << parameters->max_label_length
                 << "' characters"
                 << " for the file system '" << parameters->filesystem_type
                 << "'";
    return LabelErrorType::kLabelErrorLongName;
  }

  // Check if new volume label contains only printable ASCII characters and
  // none of forbidden.
  for (char value : volume_label) {
    if (!std::isprint(value) || std::strchr(kForbiddenCharacters, value)) {
      LOG(WARNING) << "New volume label '" << volume_label << "' contains "
                   << "forbidden character: '" << value << "'";
      return LabelErrorType::kLabelErrorInvalidCharacter;
    }
  }

  return LabelErrorType::kLabelErrorNone;
}

}  // namespace cros_disks
