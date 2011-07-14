// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SUPPLICANT_PROXY_FACTORY_H_
#define SUPPLICANT_PROXY_FACTORY_H_

#include <dbus-c++/dbus.h>

#include "shill/refptr_types.h"

namespace shill {

class SupplicantInterfaceProxyInterface;
class SupplicantProcessProxyInterface;

class SupplicantProxyFactory {
 public:
  SupplicantProxyFactory();
  virtual ~SupplicantProxyFactory();

  virtual SupplicantProcessProxyInterface *CreateProcessProxy(
      const char *dbus_path, const char *dbus_addr);

  virtual SupplicantInterfaceProxyInterface *CreateInterfaceProxy(
      const WiFiRefPtr &wifi,
      const ::DBus::Path &object_path,
      const char *dbus_addr);

 private:
  DISALLOW_COPY_AND_ASSIGN(SupplicantProxyFactory);
};

}  // namespace shill

#endif  // SUPPLICANT_PROXY_FACTORY_H_
