// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_WIMAX_WIMAX_MANAGER_PROXY_INTERFACE_H_
#define SHILL_WIMAX_WIMAX_MANAGER_PROXY_INTERFACE_H_

#include <vector>

#include <base/callback.h>

#include "shill/accessor_interface.h"

namespace shill {

class Error;

// These are the methods that a WiMaxManager proxy must support. The interface
// is provided so that it can be mocked in tests.
class WiMaxManagerProxyInterface {
 public:
  typedef base::Callback<void(const RpcIdentifiers&)> DevicesChangedCallback;

  virtual ~WiMaxManagerProxyInterface() {}

  virtual void set_devices_changed_callback(
      const DevicesChangedCallback& callback) = 0;

  // Properties.
  virtual RpcIdentifiers Devices(Error* error) = 0;
};

}  // namespace shill

#endif  // SHILL_WIMAX_WIMAX_MANAGER_PROXY_INTERFACE_H_
