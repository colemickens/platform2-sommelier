// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular/mock_mm1_bearer_proxy.h"

#include "shill/testing.h"

using testing::_;

namespace shill {
namespace mm1 {

MockBearerProxy::MockBearerProxy() {
  ON_CALL(*this, Connect(_, _, _))
      .WillByDefault(SetOperationFailedInArgumentAndWarn<0>());
  ON_CALL(*this, Disconnect(_, _, _))
      .WillByDefault(SetOperationFailedInArgumentAndWarn<0>());
}

MockBearerProxy::~MockBearerProxy() = default;

}  // namespace mm1
}  // namespace shill
