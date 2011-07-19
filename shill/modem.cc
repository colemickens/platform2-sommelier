// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/modem.h"

#include "shill/dbus_properties_proxy_interface.h"
#include "shill/proxy_factory.h"

namespace shill {

Modem::Modem(const std::string &owner,
             const std::string &path,
             ControlInterface *control_interface,
             EventDispatcher *dispatcher,
             Manager *manager)
    : dbus_properties_proxy_(
        ProxyFactory::factory()->CreateDBusPropertiesProxy(this, path, owner)),
      control_interface_(control_interface),
      dispatcher_(dispatcher),
      manager_(manager) {}

Modem::~Modem() {}

}  // namespace shill
