// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FIDES_TEST_HELPERS_H_
#define FIDES_TEST_HELPERS_H_

#include <iostream>

#include "fides/key.h"

namespace fides {

// A gtest print helper for Keys.
void PrintTo(const Key& key, std::ostream* os);

}  // namespace fides

#endif  // FIDES_TEST_HELPERS_H_
