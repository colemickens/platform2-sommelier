// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_WIMAX_H_
#define SHILL_WIMAX_H_

#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/device.h"
#include "shill/wimax_network_proxy_interface.h"

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
  virtual bool Load(StoreInterface *storage);

  virtual void ConnectTo(const WiMaxServiceRefPtr &service, Error *error);
  virtual void DisconnectFrom(const WiMaxServiceRefPtr &service, Error *error);

  // Finds or creates a service with the given parameters. The parameters
  // uniquely identify a service so no duplicate services will be created.
  WiMaxServiceRefPtr GetService(const WiMaxNetworkId &id,
                                const std::string &name);

  // Starts all services with network ids in the current set of live
  // networks. This method also creates, registers and starts the default
  // service for each live network.
  virtual void StartLiveServices();

  const RpcIdentifier &path() const { return path_; }
  bool scanning() const { return scanning_; }

 private:
  friend class WiMaxTest;
  FRIEND_TEST(WiMaxProviderTest, GetService);
  FRIEND_TEST(WiMaxTest, DestroyAllServices);
  FRIEND_TEST(WiMaxTest, FindService);
  FRIEND_TEST(WiMaxTest, GetDefaultService);
  FRIEND_TEST(WiMaxTest, GetService);
  FRIEND_TEST(WiMaxTest, LoadServices);
  FRIEND_TEST(WiMaxTest, OnNetworksChanged);
  FRIEND_TEST(WiMaxTest, StartLiveServicesForNetwork);
  FRIEND_TEST(WiMaxTest, StartStop);
  FRIEND_TEST(WiMaxTest, StopDeadServices);

  static const int kTimeoutDefault;

  void OnScanNetworksComplete(const Error &error);
  void OnConnectComplete(const Error &error);
  void OnDisconnectComplete(const Error &error);
  void OnEnableComplete(const EnabledStateChangedCallback &callback,
                        const Error &error);
  void OnDisableComplete(const EnabledStateChangedCallback &callback,
                         const Error &error);

  void OnNetworksChanged(const RpcIdentifiers &networks);

  // Starts all services with a network id matching the network id of the
  // |network| RPC object. This method also creates, registers and starts the
  // default service for |network|.
  void StartLiveServicesForNetwork(const RpcIdentifier &network);

  // Stops all services with network ids that are not in the current set of live
  // networks.
  void StopDeadServices();

  // Deregisters all services from Manager and destroys them.
  void DestroyAllServices();

  // Finds to creates the default service for |network|.
  WiMaxServiceRefPtr GetDefaultService(const RpcIdentifier &network);

  // Finds and returns the service identified by |storage_id|. Returns NULL if
  // the service is not found.
  WiMaxServiceRefPtr FindService(const std::string &storage_id);

  // Creates and registers all WiMAX services available in |storage|. Returns
  // true if any services were created.
  bool LoadServices(StoreInterface *storage);

  const RpcIdentifier path_;

  scoped_ptr<WiMaxDeviceProxyInterface> proxy_;
  bool scanning_;
  RpcIdentifiers networks_;
  std::vector<WiMaxServiceRefPtr> services_;
  WiMaxServiceRefPtr pending_service_;

  ProxyFactory *proxy_factory_;

  DISALLOW_COPY_AND_ASSIGN(WiMax);
};

}  // namespace shill

#endif  // SHILL_WIMAX_H_
