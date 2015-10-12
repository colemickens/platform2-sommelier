// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBBRILLO_BRILLO_TYPE_NAME_UNDECORATE_H_
#define LIBBRILLO_BRILLO_TYPE_NAME_UNDECORATE_H_

#include <string>
#include <typeinfo>

#include <brillo/brillo_export.h>

namespace brillo {

// Use brillo::UndecorateTypeName() to obtain human-readable type from
// the decorated/mangled type name returned by std::type_info::name().
BRILLO_EXPORT std::string UndecorateTypeName(const char* type_name);

// A template helper function that returns the undecorated type name for type T.
template<typename T>
inline std::string GetUndecoratedTypeName() {
  return UndecorateTypeName(typeid(T).name());
}

}  // namespace brillo

#endif  // LIBBRILLO_BRILLO_TYPE_NAME_UNDECORATE_H_
