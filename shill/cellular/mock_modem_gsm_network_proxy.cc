// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular/mock_modem_gsm_network_proxy.h"

#include "shill/testing.h"

using testing::_;

namespace shill {

MockModemGSMNetworkProxy::MockModemGSMNetworkProxy() {
  ON_CALL(*this, GetRegistrationInfo(_, _, _))
      .WillByDefault(SetOperationFailedInArgumentAndWarn<0>());
  ON_CALL(*this, GetSignalQuality(_, _, _))
      .WillByDefault(SetOperationFailedInArgumentAndWarn<0>());
  ON_CALL(*this, Register(_, _, _, _))
      .WillByDefault(SetOperationFailedInArgumentAndWarn<1>());
  ON_CALL(*this, Scan(_, _, _))
      .WillByDefault(SetOperationFailedInArgumentAndWarn<0>());
}

MockModemGSMNetworkProxy::~MockModemGSMNetworkProxy() {}

}  // namespace shill
