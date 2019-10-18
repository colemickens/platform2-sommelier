// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_POLICY_AMBIENT_LIGHT_PREF_FUZZ_UTIL_H_
#define POWER_MANAGER_POWERD_POLICY_AMBIENT_LIGHT_PREF_FUZZ_UTIL_H_

#include <fuzzer/FuzzedDataProvider.h>

#include <string>

namespace power_manager {
namespace policy {
namespace test {

// Helper method to generate valid ambient light pref string
std::string GenerateAmbientLightPref(FuzzedDataProvider* data_provider,
                                     int max_step = 10,
                                     int lux_max = 20000);

}  // namespace test
}  // namespace policy
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_POLICY_AMBIENT_LIGHT_PREF_FUZZ_UTIL_H_
