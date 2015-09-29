// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SETTINGSD_TEST_HELPERS_H_
#define SETTINGSD_TEST_HELPERS_H_

#include <iostream>

#include "settingsd/key.h"

namespace settingsd {

// A gtest print helper for Keys.
void PrintTo(const Key& key, std::ostream* os);

}  // namespace settingsd

#endif  // SETTINGSD_TEST_HELPERS_H_
