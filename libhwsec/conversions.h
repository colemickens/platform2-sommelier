// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBHWSEC_CONVERSIONS_H_
#define LIBHWSEC_CONVERSIONS_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace hwsec {

// Type conversion from C string buffer (char*) to bytes buffer (uint8_t*)
inline const uint8_t* CStringAsBytesByffer(const char* str) {
  return reinterpret_cast<const uint8_t*>(str);
}

inline std::string BytesToString(const std::vector<uint8_t>& bytes) {
  return std::string(bytes.begin(), bytes.end());
}

inline base::Optional<std::string> BytesToString(
    const base::Optional<std::vector<uint8_t>>& maybe_bytes) {
  if (maybe_bytes == base::nullopt) {
    return base::nullopt;
  }
  return BytesToString(*maybe_bytes);
}

}  // namespace hwsec

#endif  // LIBHWSEC_CONVERSIONS_H_
