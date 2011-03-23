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
  MOCK_METHOD2(GetBrightness, bool(int64* level, int64* max));
  MOCK_METHOD1(GetTargetBrightness, bool(int64* level));
  MOCK_METHOD1(SetBrightness, bool(int64 level));
  MOCK_METHOD2(SetScreenOffFunc, void(SIGNAL_CALLBACK_PTR(void, func),
                                      void *data));
};

}  // namespace power_manager

#endif  // POWER_MANAGER_MOCK_BACKLIGHT_H_
