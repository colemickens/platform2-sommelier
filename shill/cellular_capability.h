// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CELLULAR_CAPABILITY_
#define SHILL_CELLULAR_CAPABILITY_

#include <base/basictypes.h>

namespace shill {

class Cellular;
class ProxyFactory;

// Cellular devices instantiate subclasses of CellularCapability that handle the
// specific modem technologies and capabilities.
class CellularCapability {
 public:
  // |cellular| is the parent Cellular device.
  CellularCapability(Cellular *cellular);
  virtual ~CellularCapability();

  Cellular *cellular() const { return cellular_; }
  ProxyFactory *proxy_factory() const { return proxy_factory_; }

  // Initialize RPC proxies.
  virtual void InitProxies() = 0;

 private:
  Cellular *cellular_;
  ProxyFactory *proxy_factory_;

  DISALLOW_COPY_AND_ASSIGN(CellularCapability);
};

}  // namespace shill

#endif  // SHILL_CELLULAR_CAPABILITY_
