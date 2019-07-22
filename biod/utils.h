// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BIOD_UTILS_H_
#define BIOD_UTILS_H_

#include <type_traits>

namespace biod {

// This function can be used to fetch the value of an enum
// typecasted to it's base type (int if none specified).
//
// enum class FlockSize : uint8_t {
//   kOne = 1,
//   kTwo,
//   ...
// };
//
// uint8_t total_animals = to_utype(FlockSize::kTwo);
template <typename E>
constexpr auto to_utype(E enumerator) noexcept {
  return static_cast<std::underlying_type_t<E>>(enumerator);
}

}  // namespace biod

#endif  // BIOD_UTILS_H_
