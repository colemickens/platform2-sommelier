// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_CROS_HEALTHD_UTILS_FILE_UTILS_H_
#define DIAGNOSTICS_CROS_HEALTHD_UTILS_FILE_UTILS_H_

#include <string>

#include <base/files/file_path.h>

namespace diagnostics {

// Reads the contents of |filename| within |directory| into |out|, trimming
// trailing whitespace. Returns true on success.
bool ReadAndTrimString(const base::FilePath& directory,
                       const std::string& filename,
                       std::string* out);

// Reads a 32-bit hex-encoded unsigned integer value from a file and returns
// true on success.
bool ReadHexUint32(const base::FilePath& directory,
                   const std::string& filename,
                   uint32_t* out);

// Reads a 64-bit hex-encoded unsigned integer value from a text file and
// returns true on success.
bool ReadHexUInt64(const base::FilePath& directory,
                   const std::string& filename,
                   uint64_t* out);

// Reads a 64-bit decimal-encoded integer value from a text file and returns
// true on success.
bool ReadInt64(const base::FilePath& directory,
               const std::string& filename,
               int64_t* out);

}  // namespace diagnostics

#endif  // DIAGNOSTICS_CROS_HEALTHD_UTILS_FILE_UTILS_H_
