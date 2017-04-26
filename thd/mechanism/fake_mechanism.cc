// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "thd/mechanism/fake_mechanism.h"

#include <base/logging.h>

namespace thd {

FakeMechanism::FakeMechanism(int64_t max_level,
                             int64_t min_level,
                             int64_t default_level,
                             const std::string& name)
    : max_level_(max_level),
      min_level_(min_level),
      default_level_(default_level),
      name_(name) {}

FakeMechanism::~FakeMechanism() {}

bool FakeMechanism::SetLevel(int64_t level) {
  if (level < min_level_ || level > max_level_) {
    LOG(WARNING) << name_ << " level " << level << " outside of range ["
                 << min_level_ << ", " << max_level_ << "]";
    return false;
  }
  LOG(INFO) << name_ << " set to " << level;
  current_level_ = level;
  return true;
}

int64_t FakeMechanism::GetCurrentLevel() {
  return current_level_;
}

int64_t FakeMechanism::GetDefaultLevel() {
  return default_level_;
}

int64_t FakeMechanism::GetMaxLevel() {
  return max_level_;
}

int64_t FakeMechanism::GetMinLevel() {
  return min_level_;
}

}  // namespace thd
