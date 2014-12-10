// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APMANAGER_SHILL_PROXY_H_
#define APMANAGER_SHILL_PROXY_H_

#include <set>
#include <string>

#include <base/macros.h>
#include <base/memory/scoped_ptr.h>

#include "shill/dbus-proxies.h"

// Proxy for shill "org.chromium.flimflam" DBus service.
namespace apmanager {

class ShillProxy {
 public:
  ShillProxy();
  virtual ~ShillProxy();

  void Init(const scoped_refptr<dbus::Bus>& bus);

  // Claim the given interface |interface_name| from shill.
  virtual void ClaimInterface(const std::string& interface_name);
  // Release the given interface |interface_name| to shill.
  virtual void ReleaseInterface(const std::string& interface_name);

 private:
  void OnServiceAvailable(bool service_available);
  void OnServiceNameChanged(const std::string& old_owner,
                            const std::string& new_owner);

  static const char kManagerPath[];

  // Bus object for system bus.
  scoped_refptr<dbus::Bus> bus_;
  // DBus proxy for shill manager.
  std::unique_ptr<org::chromium::flimflam::ManagerProxy> manager_proxy_;
  // List of interfaces apmanager have claimed.
  std::set<std::string> claimed_interfaces_;

  DISALLOW_COPY_AND_ASSIGN(ShillProxy);
};

}  // namespace apmanager

#endif  // APMANAGER_SHILL_PROXY_H_
