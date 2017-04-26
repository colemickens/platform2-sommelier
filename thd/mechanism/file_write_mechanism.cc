// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "thd/mechanism/file_write_mechanism.h"

#include <base/files/file_util.h>
#include <base/logging.h>

namespace thd {

FileWriteMechanism::FileWriteMechanism(int64_t max_level,
                                       int64_t min_level,
                                       int64_t default_level,
                                       const std::string& name,
                                       const base::FilePath& path)
    : max_level_(max_level),
      min_level_(min_level),
      default_level_(default_level),
      name_(name),
      path_(path) {}

FileWriteMechanism::~FileWriteMechanism() {}

bool FileWriteMechanism::SetLevel(int64_t level) {
  if (level < min_level_ || level > max_level_) {
    LOG(WARNING) << name_ << " level " << level << " outside of range ["
                 << min_level_ << ", " << max_level_ << "]";
    return false;
  }
  std::string level_str = std::to_string(level);
  if (base::WriteFile(path_, level_str.c_str(), level_str.length()) == -1) {
    PLOG(ERROR) << name_ << " unable to write " << level_str << " to path "
                << path_.value() << ".";
    return false;
  }
  return true;
}

int64_t FileWriteMechanism::GetMaxLevel() {
  return max_level_;
}

int64_t FileWriteMechanism::GetMinLevel() {
  return min_level_;
}

int64_t FileWriteMechanism::GetDefaultLevel() {
  return default_level_;
}

}  // namespace thd
