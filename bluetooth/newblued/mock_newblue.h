// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLUETOOTH_NEWBLUED_MOCK_NEWBLUE_H_
#define BLUETOOTH_NEWBLUED_MOCK_NEWBLUE_H_

#include <gmock/gmock.h>

#include "bluetooth/newblued/newblue.h"

namespace bluetooth {

class MockNewblue : public Newblue {
 public:
  using Newblue::Newblue;

  MOCK_METHOD0(Init, bool());
};

}  // namespace bluetooth

#endif  // BLUETOOTH_NEWBLUED_MOCK_NEWBLUE_H_
