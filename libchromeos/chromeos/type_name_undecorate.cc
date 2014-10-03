// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chromeos/type_name_undecorate.h>

#ifdef __GNUG__
#include <cstdlib>
#include <cxxabi.h>
#include <memory>
#endif  // __GNUG__

namespace chromeos {

std::string UndecorateTypeName(const char* type_name) {
#ifdef __GNUG__
  // Under g++ use abi::__cxa_demangle() to undecorate the type name.
  int status = 0;

  std::unique_ptr<char, decltype(&std::free)> res{
      abi::__cxa_demangle(type_name, nullptr, nullptr, &status),
      std::free
  };

  return (status == 0) ? res.get() : type_name;
#else
  // If not compiled with g++, do nothing...
  // E.g. MSVC's type_info::name() already contains undecorated name.
  return type_name;
#endif
}

}  // namespace chromeos
