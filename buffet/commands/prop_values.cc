// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/commands/prop_values.h"

#include <memory>

#include <base/values.h>

#include "buffet/commands/prop_types.h"

namespace buffet {

// Specializations of generic GetValueType<T>() for supported C++ types.
template<> ValueType GetValueType<int>() { return ValueType::Int; }
template<> ValueType GetValueType<double>() { return ValueType::Double; }
template<> ValueType GetValueType<std::string>() { return ValueType::String; }
template<> ValueType GetValueType<bool>() { return ValueType::Boolean; }

PropValue::~PropValue() {}

}  // namespace buffet
