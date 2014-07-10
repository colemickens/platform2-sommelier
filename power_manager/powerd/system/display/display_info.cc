// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/display/display_info.h"

namespace power_manager {
namespace system {

DisplayInfo::DisplayInfo() {}

DisplayInfo::~DisplayInfo() {}

bool DisplayInfo::operator<(const DisplayInfo& rhs) const {
  if (drm_path.value() < rhs.drm_path.value())
    return true;
  if (drm_path.value() > rhs.drm_path.value())
    return false;
  return i2c_path.value() < rhs.i2c_path.value();
}

bool DisplayInfo::operator==(const DisplayInfo& o) const {
  return !(*this < o || o < *this);
}

}  // namespace system
}  // namespace power_manager
