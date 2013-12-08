// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/backlight_stub.h"

namespace power_manager {
namespace system {

BacklightStub::BacklightStub(int64 max_level, int64 current_level)
    : max_level_(max_level),
      current_level_(current_level),
      resume_level_(-1),
      should_fail_(false) {
}

BacklightStub::~BacklightStub() {}

int64 BacklightStub::GetMaxBrightnessLevel() {
  return max_level_;
}

int64 BacklightStub::GetCurrentBrightnessLevel() {
  return current_level_;
}

bool BacklightStub::SetBrightnessLevel(int64 level, base::TimeDelta interval) {
  if (should_fail_)
    return false;
  current_level_ = level;
  current_interval_ = interval;
  return true;
}

bool BacklightStub::SetResumeBrightnessLevel(int64 level) {
  if (should_fail_)
    return false;
  resume_level_ = level;
  return true;
}

}  // namespace system
}  // namespace power_manager
