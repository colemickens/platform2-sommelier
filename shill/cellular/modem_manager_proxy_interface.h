// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CELLULAR_MODEM_MANAGER_PROXY_INTERFACE_H_
#define SHILL_CELLULAR_MODEM_MANAGER_PROXY_INTERFACE_H_

#include <string>
#include <vector>

namespace shill {

// These are the methods that a ModemManager proxy must support. The interface
// is provided so that it can be mocked in tests.
class ModemManagerProxyInterface {
 public:
  virtual ~ModemManagerProxyInterface() {}

  virtual std::vector<std::string> EnumerateDevices() = 0;
};

}  // namespace shill

#endif  // SHILL_CELLULAR_MODEM_MANAGER_PROXY_INTERFACE_H_
