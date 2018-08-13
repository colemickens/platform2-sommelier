// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLUETOOTH_NEWBLUED_MOCK_NEWBLUE_H_
#define BLUETOOTH_NEWBLUED_MOCK_NEWBLUE_H_

#include <string>

#include <gmock/gmock.h>

#include "bluetooth/newblued/newblue.h"

namespace bluetooth {

class MockNewblue : public Newblue {
 public:
  using Newblue::Newblue;

  MOCK_METHOD0(Init, bool());
  MOCK_METHOD1(ListenReadyForUp, bool(base::Closure));
  MOCK_METHOD0(BringUp, bool());

  MOCK_METHOD1(StartDiscovery, bool(DeviceDiscoveredCallback));
  MOCK_METHOD0(StopDiscovery, bool());

  MOCK_METHOD1(Pair, bool(const std::string&));
  MOCK_METHOD1(CancelPair, bool(const std::string&));
};

}  // namespace bluetooth

#endif  // BLUETOOTH_NEWBLUED_MOCK_NEWBLUE_H_
