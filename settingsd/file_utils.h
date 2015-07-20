// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SETTINGSD_FILE_UTILS_H_
#define SETTINGSD_FILE_UTILS_H_

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

namespace settingsd {

namespace utils {

// Non-recursively lists all file entries in |path|.
std::vector<std::string> ListFiles(const std::string& path);

// Non-recursively lists all directory entries in |path|. "." and ".." are not
// included in this list.
std::vector<std::string> ListDirectories(const std::string& path);

// Returns true if the given path exists on the local filesystem, false
// otherwise.
bool PathExists(const std::string& path);

// Creates a directory, as well as creating any parent directories, if they
// don't exist. Returns true on successful creation, otherwise false.
bool CreateDirectory(const std::string& path);

// Deletes the file at |path| and returns true on success. Otherwise returns
// false. If |path| refers to a directory, the directory remains unchanged and
// returns false.
bool DeleteFile(const std::string& path);

// Reads the file at |path| into |contents| and returns true on success.
// Otherwise returns false. When the file size exceeds |max_size|, the
// function returns false with |contents| unaltered. |contents| may be NULL.
bool ReadFile(const std::string& path,
              std::vector<uint8_t>* contents,
              size_t max_size);

// Save |data| to |path| in an atomic manner. We achieve this by writing to a
// temporary file. Only after that write is successful, we rename the temporary
// file to target filename, call fdatasync() and return.
bool WriteFileAtomically(const std::string& path,
                         const std::vector<uint8_t>& data);

}  // namespace utils

}  // namespace settingsd

#endif  // SETTINGSD_FILE_UTILS_H_
