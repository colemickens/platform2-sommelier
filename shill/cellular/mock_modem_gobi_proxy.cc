// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular/mock_modem_gobi_proxy.h"

#include "shill/testing.h"

using testing::_;

namespace shill {

MockModemGobiProxy::MockModemGobiProxy() {
  ON_CALL(*this, SetCarrier(_, _, _, _))
      .WillByDefault(SetOperationFailedInArgumentAndWarn<1>());
}

MockModemGobiProxy::~MockModemGobiProxy() {}

}  // namespace shill
