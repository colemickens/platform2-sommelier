// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_WIMAX_PROVIDER_H_
#define SHILL_WIMAX_PROVIDER_H_

#include <map>
#include <vector>

#include <base/basictypes.h>
#include <base/memory/scoped_ptr.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/accessor_interface.h"
#include "shill/refptr_types.h"

namespace shill {

class ControlInterface;
class EventDispatcher;
class KeyValueStore;
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

  // Used by Manager::GetService.
  WiMaxServiceRefPtr GetService(const KeyValueStore &args, Error *error);

 private:
  friend class WiMaxProviderTest;
  FRIEND_TEST(WiMaxProviderTest, CreateDevice);
  FRIEND_TEST(WiMaxProviderTest, DestroyDeadDevices);
  FRIEND_TEST(WiMaxProviderTest, GetLinkName);
  FRIEND_TEST(WiMaxProviderTest, GetService);
  FRIEND_TEST(WiMaxProviderTest, OnDeviceInfoAvailable);
  FRIEND_TEST(WiMaxProviderTest, OnDevicesChanged);
  FRIEND_TEST(WiMaxProviderTest, StartStop);

  void OnDevicesChanged(const RpcIdentifiers &devices);

  void CreateDevice(const std::string &link_name, const RpcIdentifier &path);
  void DestroyDeadDevices(const RpcIdentifiers &live_devices);

  std::string GetLinkName(const RpcIdentifier &path);

  ControlInterface *control_;
  EventDispatcher *dispatcher_;
  Metrics *metrics_;
  Manager *manager_;

  scoped_ptr<WiMaxManagerProxyInterface> manager_proxy_;

  std::map<std::string, std::string> pending_devices_;
  std::map<std::string, WiMaxRefPtr> devices_;

  ProxyFactory *proxy_factory_;

  DISALLOW_COPY_AND_ASSIGN(WiMaxProvider);
};

}  // namespace shill

#endif  // SHILL_WIMAX_PROVIDER_H_
