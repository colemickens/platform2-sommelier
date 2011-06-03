// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DHCP_PROVIDER_
#define SHILL_DHCP_PROVIDER_

#include <map>

#include <base/memory/scoped_ptr.h>
#include <base/memory/singleton.h>
#include <dbus-c++/connection.h>

#include "shill/dhcp_config.h"

namespace shill {

class DHCPListenerInterface;
class Device;

// DHCPProvider is a singleton providing the main DHCP configuration
// entrypoint. Once the provider is initialized through its Init method, DHCP
// configurations for devices can be obtained through its CreateConfig
// method. For example, a single DHCP configuration request can be initiated as:
//
// DHCPProvider::GetInstance()->CreateConfig(device)->Request();
class DHCPProvider {
 public:
  // This is a singleton -- use DHCPProvider::GetInstance()->Foo()
  static DHCPProvider *GetInstance();

  // Initializes the provider singleton. This method hooks up a D-Bus signal
  // listener to |connection| that catches signals from spawned DHCP clients and
  // dispatches them to the appropriate DHCP configuration instance.
  void Init(DBus::Connection *connection);

  // Creates a new DHCPConfig for |device|. The DHCP configuration for the
  // device can then be initiated through DHCPConfig::Request and
  // DHCPConfig::Renew.
  DHCPConfigRefPtr CreateConfig(const Device &device);

  // Returns the DHCP configuration associated with DHCP client |pid|. Return
  // NULL if |pid| is not bound to a configuration.
  DHCPConfigRefPtr GetConfig(unsigned int pid);

  // Binds a |pid| to a DHCP |config|. When a DHCP config spawns a new DHCP
  // client, it binds itself to that client's |pid|.
  void BindPID(unsigned int pid, DHCPConfigRefPtr config);

  // Unbinds a |pid|. This method is used by a DHCP config to signal the
  // provider that the DHCP client has been terminated. This may result in
  // destruction of the DHCP config instance if its reference count goes to 0.
  void UnbindPID(unsigned int pid);

 private:
  friend struct DefaultSingletonTraits<DHCPProvider>;
  typedef std::map<unsigned int, DHCPConfigRefPtr> PIDConfigMap;

  // Private to ensure that this behaves as a singleton.
  DHCPProvider();
  virtual ~DHCPProvider();

  // A single listener is used to catch signals from all DHCP clients and
  // dispatch them to the appropriate proxy.
  scoped_ptr<DHCPListenerInterface> listener_;

  // A map that binds PIDs to DHCP configuration instances.
  PIDConfigMap configs_;

  DISALLOW_COPY_AND_ASSIGN(DHCPProvider);
};

}  // namespace shill

#endif  // SHILL_DHCP_PROVIDER_
