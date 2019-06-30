// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBBRILLO_BRILLO_TYPE_LIST_H_
#define LIBBRILLO_BRILLO_TYPE_LIST_H_

#include <type_traits>

namespace brillo {

template <typename... Ts>
struct TypeList {};

namespace type_list {

template <typename... Ts>
struct is_one_of {
  static constexpr bool value = false;
};

template <typename T, typename Head, typename... Tail>
struct is_one_of<T, TypeList<Head, Tail...>> {
  static constexpr bool value =
    std::is_same<T, Head>::value || is_one_of<T, TypeList<Tail...>>::value;
};

}  // namespace type_list

// Enables a template if the type T is in the typelist Types. Since std::same is
// used to determine equivalence of types, cv-qualifiers (const and volatile)
// *are* important. Note that typedefs and type aliases do not define new types.
//
// Example:
//  using ValidTypes = TypeList<int32_t, float>;
//
//  template <typename T, typename = EnableIfIsOneOf<T, ValidTypes>>
//  void f(){}
//
//  using integer = int32_t;
//  ...
//  f<int32_t>();        // Fine.
//  f<float>();          // Fine.
//  f<integer>();        // Fine.
//  f<const int32_t>();  // Error; no matching function for call to 'f'.
//  f<uint32_t>();       // Error; no matching function for call to 'f'.
template <typename T, typename Types>
using EnableIfIsOneOf =
  std::enable_if_t<type_list::is_one_of<T, Types>::value>;

}  // namespace brillo

#endif  // LIBBRILLO_BRILLO_TYPE_LIST_H_
