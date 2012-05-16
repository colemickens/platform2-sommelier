// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_WIMAX_PROVIDER_H_
#define SHILL_WIMAX_PROVIDER_H_

#include <vector>

#include <base/basictypes.h>
#include <base/memory/scoped_ptr.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/accessor_interface.h"
#include "shill/dbus_properties.h"
#include "shill/refptr_types.h"

namespace shill {

class ControlInterface;
class DBusPropertiesProxyInterface;
class EventDispatcher;
class Manager;
class Metrics;
class ProxyFactory;
class WiMaxManagerProxyInterface;

class WiMaxProvider {
 public:
  WiMaxProvider(ControlInterface *control,
                EventDispatcher *dispatcher,
                Metrics *metrics,
                Manager *manager);
  virtual ~WiMaxProvider();

  virtual void Start();
  virtual void Stop();

  virtual void OnDeviceInfoAvailable(const std::string &link_name);

 private:
  friend class WiMaxProviderTest;
  FRIEND_TEST(WiMaxProviderTest, CreateDevice);
  FRIEND_TEST(WiMaxProviderTest, DestroyDeadDevices);
  FRIEND_TEST(WiMaxProviderTest, GetLinkName);
  FRIEND_TEST(WiMaxProviderTest, OnDeviceInfoAvailable);
  FRIEND_TEST(WiMaxProviderTest, OnPropertiesChanged);
  FRIEND_TEST(WiMaxProviderTest, StartStop);
  FRIEND_TEST(WiMaxProviderTest, UpdateDevices);

  typedef std::vector<RpcIdentifier> RpcIdentifiers;

  void UpdateDevices(const RpcIdentifiers &devices);

  void OnPropertiesChanged(
      const std::string &interface,
      const DBusPropertiesMap &changed_properties,
      const std::vector<std::string> &invalidated_properties);

  void CreateDevice(const std::string &link_name, const RpcIdentifier &path);
  void DestroyDeadDevices(const RpcIdentifiers &live_devices);

  std::string GetLinkName(const RpcIdentifier &path);

  ControlInterface *control_;
  EventDispatcher *dispatcher_;
  Metrics *metrics_;
  Manager *manager_;

  scoped_ptr<WiMaxManagerProxyInterface> manager_proxy_;
  scoped_ptr<DBusPropertiesProxyInterface> properties_proxy_;

  std::map<std::string, std::string> pending_devices_;
  std::vector<WiMaxRefPtr> devices_;

  ProxyFactory *proxy_factory_;

  DISALLOW_COPY_AND_ASSIGN(WiMaxProvider);
};

}  // namespace shill

#endif  // SHILL_WIMAX_PROVIDER_H_
