// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROS_DISKS_FILESYSTEM_LABEL_H_
#define CROS_DISKS_FILESYSTEM_LABEL_H_

#include <string>

namespace cros_disks {

enum class LabelErrorType {
  kLabelErrorNone = 0,
  kLabelErrorUnsupportedFilesystem = 1,
  kLabelErrorLongName = 2,
  kLabelErrorInvalidCharacter = 3,
};

// Returns true if the file system type is supported, and |volume_label|
// contains only allowed characters and length is not greater than the file
// system's limit.
LabelErrorType ValidateVolumeLabel(const std::string& volume_label,
                                   const std::string& filesystem_type);

}  // namespace cros_disks

#endif  // CROS_DISKS_FILESYSTEM_LABEL_H_
