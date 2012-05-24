// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_WIMAX_H_
#define SHILL_WIMAX_H_

#include <set>

#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/device.h"

namespace shill {

class ProxyFactory;
class WiMaxDeviceProxyInterface;

class WiMax : public Device {
 public:
  WiMax(ControlInterface *control,
        EventDispatcher *dispatcher,
        Metrics *metrics,
        Manager *manager,
        const std::string &link_name,
        const std::string &address,
        int interface_index,
        const RpcIdentifier &path);

  virtual ~WiMax();

  // Inherited from Device.
  virtual void Start(Error *error, const EnabledStateChangedCallback &callback);
  virtual void Stop(Error *error, const EnabledStateChangedCallback &callback);
  virtual bool TechnologyIs(const Technology::Identifier type) const;
  virtual void Scan(Error *error);

  virtual void ConnectTo(const WiMaxServiceRefPtr &service, Error *error);
  virtual void DisconnectFrom(const ServiceRefPtr &service, Error *error);

  // Signaled by |service| when stopped.
  virtual void OnServiceStopped(const WiMaxServiceRefPtr &service);

  const RpcIdentifier &path() const { return path_; }
  bool scanning() const { return scanning_; }
  const std::set<RpcIdentifier> &networks() const { return networks_; }

 private:
  friend class WiMaxTest;
  FRIEND_TEST(WiMaxProviderTest, OnNetworksChanged);
  FRIEND_TEST(WiMaxTest, OnNetworksChanged);
  FRIEND_TEST(WiMaxTest, OnServiceStopped);
  FRIEND_TEST(WiMaxTest, StartStop);

  static const int kTimeoutDefault;

  void OnScanNetworksComplete(const Error &error);
  void OnConnectComplete(const Error &error);
  void OnDisconnectComplete(const Error &error);
  void OnEnableComplete(const EnabledStateChangedCallback &callback,
                        const Error &error);
  void OnDisableComplete(const EnabledStateChangedCallback &callback,
                         const Error &error);

  void OnNetworksChanged(const RpcIdentifiers &networks);

  void DropConnection();

  const RpcIdentifier path_;

  scoped_ptr<WiMaxDeviceProxyInterface> proxy_;
  bool scanning_;
  WiMaxServiceRefPtr pending_service_;
  std::set<RpcIdentifier> networks_;

  ProxyFactory *proxy_factory_;

  DISALLOW_COPY_AND_ASSIGN(WiMax);
};

}  // namespace shill

#endif  // SHILL_WIMAX_H_
