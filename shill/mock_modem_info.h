// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_MODEM_INFO_
#define SHILL_MOCK_MODEM_INFO_

#include <base/basictypes.h>
#include <gmock/gmock.h>

#include "shill/modem_info.h"

namespace shill {

class MockModemInfo : public ModemInfo {
 public:
  MockModemInfo();
  virtual ~MockModemInfo();

  MOCK_METHOD0(Start, void());
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD1(OnDeviceInfoAvailable, void(const std::string &link_name));

  DISALLOW_COPY_AND_ASSIGN(MockModemInfo);
};

}  // namespace shill

#endif  // SHILL_MOCK_MODEM_INFO_
