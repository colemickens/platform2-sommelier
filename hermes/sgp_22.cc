// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "hermes/sgp_22.h"

#include <array>

namespace hermes {

// Application identifier for the eUICC's ISD-R, as per SGP.02 2.2.3
extern const std::array<uint8_t, 16> kAidIsdr = {
    0xA0, 0x00, 0x00, 0x05, 0x59, 0x10, 0x10, 0xFF,
    0xFF, 0xFF, 0xFF, 0x89, 0x00, 0x00, 0x01, 0x00,
};

// Application identifier for the eUICC's ECASD, as per SGP.02 2.2.3
extern const std::array<uint8_t, 16> kAidEcasd = {
    0xA0, 0x00, 0x00, 0x05, 0x59, 0x10, 0x10, 0xFF,
    0xFF, 0xFF, 0xFF, 0x89, 0x00, 0x00, 0x02, 0x00,
};

}  // namespace hermes
