// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_IPCONFIG_H_
#define SHILL_IPCONFIG_H_

#include <string>
#include <vector>

#include <base/callback.h>
#include <base/memory/ref_counted.h>
#include <base/memory/scoped_ptr.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/ip_address.h"
#include "shill/property_store.h"
#include "shill/refptr_types.h"
#include "shill/routing_table_entry.h"

namespace shill {
class ControlInterface;
class Error;
class IPConfigAdaptorInterface;
class StaticIPParameters;

// IPConfig superclass. Individual IP configuration types will inherit from this
// class.
class IPConfig : public base::RefCounted<IPConfig> {
 public:
  struct Route {
    std::string host;
    std::string netmask;
    std::string gateway;
  };

  struct Properties {
    Properties() : address_family(IPAddress::kFamilyUnknown),
                   subnet_prefix(0),
                   blackhole_ipv6(false),
                   mtu(0),
                   lease_duration_seconds(0) {}

    IPAddress::Family address_family;
    std::string address;
    int32 subnet_prefix;
    std::string broadcast_address;
    std::vector<std::string> dns_servers;
    std::string domain_name;
    std::vector<std::string> domain_search;
    std::string gateway;
    std::string method;
    std::string peer_address;
    // Used by OpenVPN to signify a destination that should bypass any default
    // route installed.  This is usually the external IP address of the VPN
    // server.
    std::string trusted_ip;
    bool blackhole_ipv6;
    int32 mtu;
    std::vector<Route> routes;
    // Vendor encapsulated option string gained from DHCP.
    std::string vendor_encapsulated_options;
    // Web Proxy Auto Discovery (WPAD) URL gained from DHCP.
    std::string web_proxy_auto_discovery;
    // Length of time the lease was granted.
    uint32 lease_duration_seconds;
  };

  enum Method {
    kMethodUnknown,
    kMethodPPP,
    kMethodStatic,
    kMethodDHCP
  };

  enum ReleaseReason {
    kReleaseReasonDisconnect,
    kReleaseReasonStaticIP
  };

  typedef base::Callback<void(const IPConfigRefPtr&)> Callback;

  IPConfig(ControlInterface *control_interface, const std::string &device_name);
  IPConfig(ControlInterface *control_interface,
           const std::string &device_name,
           const std::string &type);
  virtual ~IPConfig();

  const std::string &device_name() const { return device_name_; }
  const std::string &type() const { return type_; }
  uint serial() const { return serial_; }

  std::string GetRpcIdentifier();

  // Registers a callback that's executed every time the configuration
  // properties are acquired. Takes ownership of |callback|.  Pass NULL
  // to remove a callback. The callback's argument is a pointer to this IP
  // configuration instance allowing clients to more easily manage multiple IP
  // configurations.
  void RegisterUpdateCallback(const Callback &callback);

  // Registers a callback that's executed every time the configuration
  // properties fail to be acquired. Takes ownership of |callback|.  Pass NULL
  // to remove a callback. The callback's argument is a pointer to this IP
  // configuration instance allowing clients to more easily manage multiple IP
  // configurations.
  void RegisterFailureCallback(const Callback &callback);

  // Registers a callback that's executed every time the Refresh method
  // on the ipconfig is called.  Takes ownership of |callback|. Pass NULL
  // to remove a callback. The callback's argument is a pointer to this IP
  // configuration instance allowing clients to more easily manage multiple IP
  // configurations.
  void RegisterRefreshCallback(const Callback &callback);

  // Registers a callback that's executed every time the the lease exipres
  // and the IPConfig is about to perform a restart to attempt to regain it.
  // Takes ownership of |callback|. Pass NULL  to remove a callback. The
  // callback's argument is a pointer to this IP configuration instance
  // allowing clients to more easily manage multiple IP configurations.
  void RegisterExpireCallback(const Callback &callback);

  void set_properties(const Properties &props) { properties_ = props; }
  virtual const Properties &properties() const { return properties_; }

  // Update DNS servers setting for this ipconfig, this allows Chrome
  // to retrieve the new DNS servers.
  virtual void UpdateDNSServers(const std::vector<std::string> &dns_servers);

  // Reset the IPConfig properties to their default values.
  virtual void ResetProperties();

  // Request, renew and release IP configuration. Return true on success, false
  // otherwise. The default implementation always returns false indicating a
  // failure.  ReleaseIP is advisory: if we are no longer connected, it is not
  // possible to properly vacate the lease on the remote server.  Also,
  // depending on the configuration of the specific IPConfig subclass, we may
  // end up holding on to the lease so we can resume to the network lease
  // faster.
  virtual bool RequestIP();
  virtual bool RenewIP();
  virtual bool ReleaseIP(ReleaseReason reason);

  // Refresh IP configuration.  Called by the DBus Adaptor "Refresh" call.
  void Refresh(Error *error);

  PropertyStore *mutable_store() { return &store_; }
  const PropertyStore &store() const { return store_; }
  void ApplyStaticIPParameters(StaticIPParameters *static_ip_parameters);

  // Restore the fields of |properties_| to their original values before
  // static IP parameters were previously applied.
  void RestoreSavedIPParameters(StaticIPParameters *static_ip_parameters);

 protected:
  // Inform RPC listeners of changes to our properties. MAY emit
  // changes even on unchanged properties.
  virtual void EmitChanges();

  // Updates the IP configuration properties and notifies registered listeners
  // about the event.
  virtual void UpdateProperties(const Properties &properties);

  // Notifies registered listeners that the configuration process has failed.
  virtual void NotifyFailure();

  // Notifies registered listeners that the lease has expired.
  virtual void NotifyExpiry();

 private:
  friend class IPConfigAdaptorInterface;
  friend class IPConfigTest;
  friend class ConnectionTest;

  FRIEND_TEST(DeviceTest, AcquireIPConfig);
  FRIEND_TEST(DeviceTest, DestroyIPConfig);
  FRIEND_TEST(DeviceTest, IsConnectedViaTether);
  FRIEND_TEST(DeviceTest, OnIPConfigExpired);
  FRIEND_TEST(IPConfigTest, UpdateCallback);
  FRIEND_TEST(IPConfigTest, UpdateProperties);
  FRIEND_TEST(ResolverTest, Empty);
  FRIEND_TEST(ResolverTest, NonEmpty);
  FRIEND_TEST(RoutingTableTest, ConfigureRoutes);
  FRIEND_TEST(RoutingTableTest, RouteAddDelete);
  FRIEND_TEST(RoutingTableTest, RouteDeleteForeign);

  static const char kType[];

  void Init();

  static uint global_serial_;
  PropertyStore store_;
  const std::string device_name_;
  const std::string type_;
  const uint serial_;
  scoped_ptr<IPConfigAdaptorInterface> adaptor_;
  Properties properties_;
  Callback update_callback_;
  Callback failure_callback_;
  Callback refresh_callback_;
  Callback expire_callback_;

  DISALLOW_COPY_AND_ASSIGN(IPConfig);
};

}  // namespace shill

#endif  // SHILL_IPCONFIG_H_
