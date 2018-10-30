// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modemfwd/firmware_file.h"

#include <base/logging.h>

#include "modemfwd/file_decompressor.h"

namespace modemfwd {

FirmwareFile::FirmwareFile() = default;

FirmwareFile::~FirmwareFile() = default;

bool FirmwareFile::PrepareFrom(const FirmwareFileInfo& file_info) {
  switch (file_info.compression) {
    case FirmwareFileInfo::Compression::NONE:
      path_for_logging_ = file_info.firmware_path;
      path_on_filesystem_ = file_info.firmware_path;
      return true;

    case FirmwareFileInfo::Compression::XZ: {
      // A xz-compressed firmware file should end with a .xz extension.
      CHECK_EQ(file_info.firmware_path.FinalExtension(), ".xz");

      if (!temp_dir_.CreateUniqueTempDir()) {
        LOG(ERROR) << "Failed to create temporary directory for "
                      "decompressing firmware";
        return false;
      }

      // Maintains the original firmware file name with the trailing .xz
      // extension removed.
      base::FilePath actual_path = temp_dir_.GetPath().Append(
          file_info.firmware_path.BaseName().RemoveFinalExtension());

      if (!DecompressXzFile(file_info.firmware_path, actual_path)) {
        LOG(ERROR) << "Failed to decompress firmware: "
                   << file_info.firmware_path.value();
        return false;
      }
      path_for_logging_ = file_info.firmware_path;
      path_on_filesystem_ = actual_path;
      return true;
    }
  }

  NOTREACHED();
  return false;
}

}  // namespace modemfwd
