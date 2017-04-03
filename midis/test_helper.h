// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MIDIS_TEST_HELPER_H_
#define MIDIS_TEST_HELPER_H_

#include "gmock/gmock.h"
#include "midis/device_tracker.h"

namespace midis {

MATCHER_P2(DeviceMatcher, id, name, "") {
  return (id == UdevHandler::GenerateDeviceId(arg->GetCard(),
                                              arg->GetDeviceNum()) &&
          base::EqualsCaseInsensitiveASCII(arg->GetName(), name));
}

}  // namespace midis

#endif  // MIDIS_TEST_HELPER_H_
