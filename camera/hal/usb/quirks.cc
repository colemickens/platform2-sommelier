/*
 * Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hal/usb/quirks.h"

#include <map>
#include <utility>

namespace cros {

namespace {

using VidPidPair = std::pair<std::string, std::string>;
using QuirksMap = std::map<VidPidPair, uint32_t>;

const QuirksMap& GetQuirksMap() {
  static const QuirksMap kQuirksMap = {
      // Logitech Webcam Pro 9000 (b/138159048)
      {{"046d", "0809"}, kQuirkPreferMjpeg},
  };
  return kQuirksMap;
}

}  // namespace

uint32_t GetQuirks(const std::string& vid, const std::string& pid) {
  const QuirksMap& quirks_map = GetQuirksMap();
  auto it = quirks_map.find({vid, pid});
  if (it != quirks_map.end()) {
    return it->second;
  }
  return 0;
}

}  // namespace cros
