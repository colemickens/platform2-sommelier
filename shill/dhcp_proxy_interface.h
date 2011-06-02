// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DHCP_PROXY_INTERFACE_
#define SHILL_DHCP_PROXY_INTERFACE_

namespace shill {

// These are the functions that a DHCP proxy and listener must support.
class DHCPProxyInterface {
 public:
  virtual ~DHCPProxyInterface() {}
};

class DHCPListenerInterface {
 public:
  virtual ~DHCPListenerInterface() {}
};

}  // namespace shill

#endif  // SHILL_DHCP_PROXY_INTERFACE_
