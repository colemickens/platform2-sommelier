// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HERMES_SGP_22_H_
#define HERMES_SGP_22_H_

#include <array>

namespace hermes {

// Application identifier for the eUICC's ISD-R, as per SGP.02 2.2.3
extern const std::array<uint8_t, 16> kAidIsdr;
// Application identifier for the eUICC's ECASD, as per SGP.02 2.2.3
extern const std::array<uint8_t, 16> kAidEcasd;

}  // namespace hermes

#endif  // HERMES_SGP_22_H_
