// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/cros_healthd/utils/file_utils.h"

#include <base/files/file_util.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>

namespace diagnostics {

bool ReadAndTrimString(const base::FilePath& directory,
                       const std::string& filename,
                       std::string* out) {
  if (!base::ReadFileToString(directory.Append(filename), out))
    return false;

  base::TrimWhitespaceASCII(*out, base::TRIM_TRAILING, out);
  return true;
}

bool ReadHexUint32(const base::FilePath& directory,
                   const std::string& filename,
                   uint32_t* out) {
  std::string buffer;
  if (!ReadAndTrimString(directory, filename, &buffer))
    return false;

  return base::HexStringToUInt(buffer, out);
}

bool ReadHexUInt64(const base::FilePath& directory,
                   const std::string& filename,
                   uint64_t* out) {
  std::string buffer;
  if (!ReadAndTrimString(directory, filename, &buffer))
    return false;

  return base::HexStringToUInt64(buffer, out);
}

bool ReadInt64(const base::FilePath& directory,
               const std::string& filename,
               int64_t* out) {
  std::string buffer;
  if (!ReadAndTrimString(directory, filename, &buffer))
    return false;

  return base::StringToInt64(buffer, out);
}

}  // namespace diagnostics
