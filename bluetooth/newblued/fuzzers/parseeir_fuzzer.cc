// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "bluetooth/newblued/util.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t dataSize) {
  bluetooth::DeviceInfo device_info(/* has_active_discovery_client */ true,
                                    /* adv_address */ "", /* address_type */ 0,
                                    /* resolved_address */ "", /* rssi */ 0,
                                    /* reply_type */ 0);
  bluetooth::ParseEir(&device_info,
                      std::vector<uint8_t>(data, data + dataSize));
  return 0;
}
