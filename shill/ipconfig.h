// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_IPCONFIG_
#define SHILL_IPCONFIG_

#include <string>
#include <vector>

#include <base/callback.h>
#include <base/logging.h>
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
class StoreInterface;

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
                   mtu(0) {}

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
    int32 mtu;
    std::vector<Route> routes;
  };

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
  // properties change. Takes ownership of |callback|. Pass NULL to remove a
  // callback. The callback's first argument is a pointer to this IP
  // configuration instance allowing clients to more easily manage multiple IP
  // configurations. The callback's second argument is set to false if IP
  // configuration failed.
  void RegisterUpdateCallback(
      const base::Callback<void(const IPConfigRefPtr&, bool)> &callback);

  void set_properties(const Properties &props) { properties_ = props; }
  const Properties &properties() const { return properties_; }

  // Request, renew and release IP configuration. Return true on success, false
  // otherwise. The default implementation always returns false indicating a
  // failure.
  virtual bool RequestIP();
  virtual bool RenewIP();
  virtual bool ReleaseIP();

  PropertyStore *mutable_store() { return &store_; }
  const PropertyStore &store() const { return store_; }
  void ApplyStaticIPParameters(const StaticIPParameters &static_ip_parameters);

  // |id_suffix| is used to generate a storage ID that binds this instance
  // to its associated device.
  virtual bool Load(StoreInterface *storage, const std::string &id_suffix);
  virtual bool Save(StoreInterface *storage, const std::string &id_suffix);

 protected:
  // Updates the IP configuration properties and notifies registered listeners
  // about the event. |success| is set to false if the IP configuration failed.
  virtual void UpdateProperties(const Properties &properties, bool success);

  // |id_suffix| is appended to the storage id, intended to bind this instance
  // to its associated device.
  std::string GetStorageIdentifier(const std::string &id_suffix);

 private:
  friend class IPConfigAdaptorInterface;
  friend class ConnectionTest;

  FRIEND_TEST(DeviceTest, AcquireIPConfig);
  FRIEND_TEST(DeviceTest, DestroyIPConfig);
  FRIEND_TEST(IPConfigTest, UpdateCallback);
  FRIEND_TEST(IPConfigTest, UpdateProperties);
  FRIEND_TEST(ResolverTest, Empty);
  FRIEND_TEST(ResolverTest, NonEmpty);
  FRIEND_TEST(RoutingTableTest, ConfigureRoutes);
  FRIEND_TEST(RoutingTableTest, RouteAddDelete);
  FRIEND_TEST(RoutingTableTest, RouteDeleteForeign);

  static const char kStorageType[];
  static const char kType[];
  static uint global_serial_;

  PropertyStore store_;
  const std::string device_name_;
  const std::string type_;
  const uint serial_;
  scoped_ptr<IPConfigAdaptorInterface> adaptor_;
  Properties properties_;
  base::Callback<void(const IPConfigRefPtr&, bool)> update_callback_;

  void Init();

  DISALLOW_COPY_AND_ASSIGN(IPConfig);
};

}  // namespace shill

#endif  // SHILL_IPCONFIG_
