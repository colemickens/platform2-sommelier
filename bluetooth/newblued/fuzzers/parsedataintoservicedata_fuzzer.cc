// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "bluetooth/newblued/util.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t dataSize) {
  std::map<bluetooth::Uuid, std::vector<uint8_t>> service_data;
  bluetooth::ParseDataIntoServiceData(&service_data, bluetooth::kUuid16Size,
                                      data, dataSize);
  bluetooth::ParseDataIntoServiceData(&service_data, bluetooth::kUuid32Size,
                                      data, dataSize);
  bluetooth::ParseDataIntoServiceData(&service_data, bluetooth::kUuid128Size,
                                      data, dataSize);
  return 0;
}
