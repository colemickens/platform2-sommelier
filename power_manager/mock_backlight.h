// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef POWER_MANAGER_MOCK_BACKLIGHT_H_
#define POWER_MANAGER_MOCK_BACKLIGHT_H_

#include <gmock/gmock.h>

#include "power_manager/backlight_interface.h"

namespace power_manager {

class MockBacklight : public BacklightInterface {
 public:
  MOCK_METHOD1(GetCurrentBrightnessLevel, bool(int64* current_level));
  MOCK_METHOD1(GetMaxBrightnessLevel, bool(int64* max_level));
  MOCK_METHOD1(SetBrightnessLevel, bool(int64 level));
};

}  // namespace power_manager

#endif  // POWER_MANAGER_MOCK_BACKLIGHT_H_
