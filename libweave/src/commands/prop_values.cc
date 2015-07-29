// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libweave/src/commands/prop_values.h"

#include "libweave/src/commands/prop_types.h"

namespace weave {

PropValue::PropValue(const PropType& type) : type_{type.Clone()} {}

PropValue::PropValue(const PropValue& other) : PropValue{*other.type_} {}

PropValue::~PropValue() {
}

}  // namespace weave
