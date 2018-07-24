// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular/mock_modem_simple_proxy.h"

#include "shill/testing.h"

using testing::_;

namespace shill {

MockModemSimpleProxy::MockModemSimpleProxy() {
  ON_CALL(*this, GetModemStatus(_, _, _))
      .WillByDefault(SetOperationFailedInArgumentAndWarn<0>());
  ON_CALL(*this, Connect(_, _, _, _))
      .WillByDefault(SetOperationFailedInArgumentAndWarn<1>());
}

MockModemSimpleProxy::~MockModemSimpleProxy() {}

}  // namespace shill
