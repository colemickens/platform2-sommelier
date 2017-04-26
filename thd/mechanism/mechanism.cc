// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "thd/mechanism/mechanism.h"

#include <cmath>

#include <base/logging.h>

namespace thd {

Mechanism::Mechanism() {}

Mechanism::~Mechanism() {}

bool Mechanism::SetPercent(int percent) {
  if (percent < 0 || percent > 100) {
    LOG(WARNING) << " percent " << percent << " outside of range [" << 0 << ", "
                 << 100 << "]";
    return false;
  }
  const int64_t min_level = GetMinLevel();
  const int64_t max_level = GetMaxLevel();
  int64_t level_to_set =
      min_level + std::round((max_level - min_level) * percent / 100);
  return SetLevel(level_to_set);
}

}  // namespace thd
