// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_WIMAX_SERVICE_H_
#define SHILL_WIMAX_SERVICE_H_

#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/refptr_types.h"
#include "shill/service.h"

namespace shill {

class KeyValueStore;
class WiMaxNetworkProxyInterface;

class WiMaxService : public Service {
 public:
  WiMaxService(ControlInterface *control,
               EventDispatcher *dispatcher,
               Metrics *metrics,
               Manager *manager,
               const WiMaxRefPtr &wimax);
  virtual ~WiMaxService();

  // Returns the parameters to be passed to WiMaxManager.Device.Connect() when
  // connecting to the network associated with this service.
  void GetConnectParameters(KeyValueStore *parameters) const;

  // Returns the RPC object path for the WiMaxManager.Network object associated
  // with this service. Must only be called after |proxy_| is set by Start().
  RpcIdentifier GetNetworkObjectPath() const;

  // Returns true on success, false otherwise. Takes ownership of proxy,
  // regardless of the result of the operation.
  bool Start(WiMaxNetworkProxyInterface *proxy);

  const std::string &network_name() const { return network_name_; }
  uint32 network_identifier() const { return network_identifier_; }

  // Inherited from Service.
  virtual bool TechnologyIs(const Technology::Identifier type) const;
  virtual void Connect(Error *error);
  virtual void Disconnect(Error *error);
  virtual std::string GetStorageIdentifier() const;

 private:
  FRIEND_TEST(WiMaxServiceTest, GetDeviceRpcId);
  FRIEND_TEST(WiMaxServiceTest, OnSignalStrengthChanged);

  virtual std::string GetDeviceRpcId(Error *error);

  void OnSignalStrengthChanged(int strength);

  WiMaxRefPtr wimax_;
  scoped_ptr<WiMaxNetworkProxyInterface> proxy_;
  std::string storage_id_;

  uint32 network_identifier_;
  std::string network_name_;
  bool need_passphrase_;

  DISALLOW_COPY_AND_ASSIGN(WiMaxService);
};

}  // namespace shill

#endif  // SHILL_WIMAX_SERVICE_H_
