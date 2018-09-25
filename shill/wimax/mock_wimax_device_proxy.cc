// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wimax/mock_wimax_device_proxy.h"

#include "shill/testing.h"

using testing::_;

namespace shill {

MockWiMaxDeviceProxy::MockWiMaxDeviceProxy() {
  ON_CALL(*this, Enable(_, _, _))
      .WillByDefault(SetOperationFailedInArgumentAndWarn<0>());
  ON_CALL(*this, Disable(_, _, _))
      .WillByDefault(SetOperationFailedInArgumentAndWarn<0>());
  ON_CALL(*this, ScanNetworks(_, _, _))
      .WillByDefault(SetOperationFailedInArgumentAndWarn<0>());
  ON_CALL(*this, Connect(_, _, _, _, _))
      .WillByDefault(SetOperationFailedInArgumentAndWarn<2>());
  ON_CALL(*this, Disconnect(_, _, _))
      .WillByDefault(SetOperationFailedInArgumentAndWarn<0>());
}

MockWiMaxDeviceProxy::~MockWiMaxDeviceProxy() {}

}  // namespace shill
