// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libmems/iio_device.h"

#include <stdlib.h>

#include <base/strings/string_number_conversions.h>

namespace libmems {

bool IioDevice::IsSingleSensor() const {
  return ReadStringAttribute("location").has_value();
}

// static
base::Optional<int> IioDevice::GetIdAfterPrefix(const char* id_str,
                                                const char* prefix) {
  size_t id_len = strlen(id_str);
  size_t prefix_len = strlen(prefix);
  if (id_len <= prefix_len || strncmp(id_str, prefix, prefix_len) != 0) {
    return base::nullopt;
  }

  int value = 0;
  bool success = base::StringToInt(std::string(id_str + prefix_len), &value);
  if (success)
    return value;

  return base::nullopt;
}

}  // namespace libmems
