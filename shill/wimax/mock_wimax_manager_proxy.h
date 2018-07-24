// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_WIMAX_MOCK_WIMAX_MANAGER_PROXY_H_
#define SHILL_WIMAX_MOCK_WIMAX_MANAGER_PROXY_H_

#include <vector>

#include <base/macros.h>
#include <gmock/gmock.h>

#include "shill/wimax/wimax_manager_proxy_interface.h"

namespace shill {

class MockWiMaxManagerProxy : public WiMaxManagerProxyInterface {
 public:
  MockWiMaxManagerProxy();
  ~MockWiMaxManagerProxy() override;

  MOCK_METHOD1(set_devices_changed_callback,
               void(const DevicesChangedCallback& callback));
  MOCK_METHOD1(Devices, RpcIdentifiers(Error* error));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockWiMaxManagerProxy);
};

}  // namespace shill

#endif  // SHILL_WIMAX_MOCK_WIMAX_MANAGER_PROXY_H_
