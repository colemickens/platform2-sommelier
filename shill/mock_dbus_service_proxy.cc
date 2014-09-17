// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mock_dbus_service_proxy.h"

#include "shill/testing.h"

using testing::_;

namespace shill {

MockDBusServiceProxy::MockDBusServiceProxy() {
  ON_CALL(*this, GetNameOwner(_, _, _, _))
      .WillByDefault(SetOperationFailedInArgumentAndWarn<1>());
}

MockDBusServiceProxy::~MockDBusServiceProxy() {}

}  // namespace shill
