// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular/mock_modem_cdma_proxy.h"

#include "shill/testing.h"

using testing::_;

namespace shill {

MockModemCDMAProxy::MockModemCDMAProxy() {
  ON_CALL(*this, Activate(_, _, _, _))
      .WillByDefault(SetOperationFailedInArgumentAndWarn<1>());
  ON_CALL(*this, GetRegistrationState(_, _, _))
      .WillByDefault(SetOperationFailedInArgumentAndWarn<0>());
  ON_CALL(*this, GetSignalQuality(_, _, _))
      .WillByDefault(SetOperationFailedInArgumentAndWarn<0>());
}

MockModemCDMAProxy::~MockModemCDMAProxy() {}

}  // namespace shill
