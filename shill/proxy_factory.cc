// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/proxy_factory.h"

#include "shill/modem_manager_proxy.h"

namespace shill {

ProxyFactory *ProxyFactory::factory_ = NULL;

ProxyFactory::ProxyFactory() {}

ProxyFactory::~ProxyFactory() {}

ModemManagerProxyInterface *ProxyFactory::CreateModemManagerProxy(
    ModemManager *manager,
    const std::string &path,
    const std::string &service) {
  return new ModemManagerProxy(manager, path, service);
}

}  // namespace shill
