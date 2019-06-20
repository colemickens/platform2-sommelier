// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/mount_info.h"

#include <base/logging.h>
#include <base/strings/string_split.h>

#include "cros-disks/file_reader.h"

namespace {

bool IsOctalDigit(char digit) {
  return digit >= '0' && digit <= '7';
}

}  // namespace

namespace cros_disks {

// A data structure for holding information of a mount point.
struct MountInfo::MountPointData {
  std::string source_path;
  std::string mount_path;
  std::string filesystem_type;
};

MountInfo::MountInfo() = default;

MountInfo::~MountInfo() = default;

int MountInfo::ConvertOctalStringToInt(const std::string& octal) const {
  if (octal.size() == 3 && IsOctalDigit(octal[0]) && IsOctalDigit(octal[1]) &&
      IsOctalDigit(octal[2])) {
    return (octal[0] - '0') * 64 + (octal[1] - '0') * 8 + (octal[2] - '0');
  }
  return -1;
}

std::string MountInfo::DecodePath(const std::string& encoded_path) const {
  size_t encoded_path_size = encoded_path.size();
  std::string decoded_path;
  decoded_path.reserve(encoded_path_size);
  for (size_t index = 0; index < encoded_path_size; ++index) {
    char path_char = encoded_path[index];
    if ((path_char == '\\') && (index + 3 < encoded_path_size)) {
      int decimal = ConvertOctalStringToInt(encoded_path.substr(index + 1, 3));
      if (decimal != -1) {
        decoded_path.push_back(static_cast<char>(decimal));
        index += 3;
        continue;
      }
    }
    decoded_path.push_back(path_char);
  }
  return decoded_path;
}

std::vector<std::string> MountInfo::GetMountPaths(
    const std::string& source_path) const {
  std::vector<std::string> mount_paths;
  for (const auto& mount_point : mount_points_) {
    if (mount_point.source_path == source_path)
      mount_paths.push_back(mount_point.mount_path);
  }
  return mount_paths;
}

bool MountInfo::HasMountPath(const std::string& mount_path) const {
  for (const auto& mount_point : mount_points_) {
    if (mount_point.mount_path == mount_path)
      return true;
  }
  return false;
}
bool MountInfo::RetrieveFromFile(const std::string& path) {
  mount_points_.clear();

  FileReader reader;
  if (!reader.Open(base::FilePath(path))) {
    LOG(ERROR) << "Failed to retrieve mount info from '" << path << "'";
    return false;
  }

  std::string line;
  while (reader.ReadLine(&line)) {
    std::vector<std::string> tokens = base::SplitString(
        line, " ", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
    size_t num_tokens = tokens.size();
    if (num_tokens >= 10 && tokens[num_tokens - 4] == "-") {
      MountPointData mount_point;
      mount_point.source_path = DecodePath(tokens[num_tokens - 2]);
      mount_point.mount_path = DecodePath(tokens[4]);
      mount_point.filesystem_type = tokens[num_tokens - 3];
      mount_points_.push_back(mount_point);
    }
  }
  return true;
}

bool MountInfo::RetrieveFromCurrentProcess() {
  return RetrieveFromFile("/proc/self/mountinfo");
}

}  // namespace cros_disks
