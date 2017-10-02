// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MODEMFWD_MOCK_MODEM_H_
#define MODEMFWD_MOCK_MODEM_H_

#include <string>

#include <base/macros.h>
#include <gmock/gmock.h>

#include "modemfwd/modem.h"

namespace modemfwd {

class MockModem : public Modem {
 public:
  MockModem() = default;
  ~MockModem() override = default;

  MOCK_CONST_METHOD0(GetDeviceId, std::string());
  MOCK_CONST_METHOD0(GetEquipmentId, std::string());
  MOCK_CONST_METHOD0(GetCarrierId, std::string());
  MOCK_CONST_METHOD0(GetMainFirmwareVersion, std::string());
};

}  // namespace modemfwd

#endif  // MODEMFWD_MOCK_MODEM_H_
