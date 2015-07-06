// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libweave/src/commands/prop_values.h"

#include "libweave/src/commands/prop_types.h"

namespace buffet {

PropValue::PropValue(std::unique_ptr<const PropType> type)
    : type_{std::move(type)} {
}

PropValue::PropValue(const PropType* type_ptr) : type_{type_ptr->Clone()} {
}

PropValue::~PropValue() {
}

}  // namespace buffet
