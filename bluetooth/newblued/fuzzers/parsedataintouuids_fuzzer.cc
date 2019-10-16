// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "bluetooth/newblued/util.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t dataSize) {
  std::set<bluetooth::Uuid> service_uuids;
  bluetooth::ParseDataIntoUuids(&service_uuids, bluetooth::kUuid16Size, data,
                                dataSize);
  bluetooth::ParseDataIntoUuids(&service_uuids, bluetooth::kUuid32Size, data,
                                dataSize);
  bluetooth::ParseDataIntoUuids(&service_uuids, bluetooth::kUuid128Size, data,
                                dataSize);
  return 0;
}
