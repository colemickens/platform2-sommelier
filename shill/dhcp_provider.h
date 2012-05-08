// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DHCP_PROVIDER_
#define SHILL_DHCP_PROVIDER_

#include <map>
#include <string>

#include <base/lazy_instance.h>
#include <base/memory/scoped_ptr.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/refptr_types.h"

namespace shill {

class ControlInterface;
class DHCPCDListener;
class EventDispatcher;
class GLib;
class ProxyFactory;

// DHCPProvider is a singleton providing the main DHCP configuration
// entrypoint. Once the provider is initialized through its Init method, DHCP
// configurations for devices can be obtained through its CreateConfig
// method. For example, a single DHCP configuration request can be initiated as:
//
// DHCPProvider::GetInstance()->CreateConfig(device_name,
//                                           host_name,
//                                           lease_file_suffix,
//                                           arp_gateway)->Request();
class DHCPProvider {
 public:
  virtual ~DHCPProvider();

  // This is a singleton -- use DHCPProvider::GetInstance()->Foo()
  static DHCPProvider *GetInstance();

  // Initializes the provider singleton. This method hooks up a D-Bus signal
  // listener that catches signals from spawned DHCP clients and dispatches them
  // to the appropriate DHCP configuration instance.
  virtual void Init(ControlInterface *control_interface,
                    EventDispatcher *dispatcher,
                    GLib *glib);

  // Creates a new DHCPConfig for |device_name|. The DHCP configuration for the
  // device can then be initiated through DHCPConfig::Request and
  // DHCPConfig::Renew.  If |host_name| is not-empty, it is placed in the DHCP
  // request to allow the server to map the request to a specific user-named
  // origin.  The DHCP lease file will contain the suffix supplied
  // in |lease_file_suffix| if non-empty, otherwise |device_name|.  If
  // |arp_gateway| is true, the DHCP client will ARP for the gateway IP
  // address as an additional safeguard against the issued IP address being
  // in-use by another station.
  virtual DHCPConfigRefPtr CreateConfig(const std::string &device_name,
                                        const std::string &host_name,
                                        const std::string &lease_file_suffix,
                                        bool arp_gateway);

  // Returns the DHCP configuration associated with DHCP client |pid|. Return
  // NULL if |pid| is not bound to a configuration.
  DHCPConfigRefPtr GetConfig(int pid);

  // Binds a |pid| to a DHCP |config|. When a DHCP config spawns a new DHCP
  // client, it binds itself to that client's |pid|.
  void BindPID(int pid, const DHCPConfigRefPtr &config);

  // Unbinds a |pid|. This method is used by a DHCP config to signal the
  // provider that the DHCP client has been terminated. This may result in
  // destruction of the DHCP config instance if its reference count goes to 0.
  void UnbindPID(int pid);

 protected:
  DHCPProvider();

 private:
  friend struct base::DefaultLazyInstanceTraits<DHCPProvider>;
  friend class CellularTest;
  friend class DHCPProviderTest;
  friend class DeviceInfoTest;
  friend class DeviceTest;
  FRIEND_TEST(DHCPProviderTest, CreateConfig);

  typedef std::map<int, DHCPConfigRefPtr> PIDConfigMap;

  // Store cached copies of singletons for speed/ease of testing.
  ProxyFactory *proxy_factory_;

  // A single listener is used to catch signals from all DHCP clients and
  // dispatch them to the appropriate DHCP configuration instance.
  scoped_ptr<DHCPCDListener> listener_;

  // A map that binds PIDs to DHCP configuration instances.
  PIDConfigMap configs_;

  ControlInterface *control_interface_;
  EventDispatcher *dispatcher_;
  GLib *glib_;

  DISALLOW_COPY_AND_ASSIGN(DHCPProvider);
};

}  // namespace shill

#endif  // SHILL_DHCP_PROVIDER_
