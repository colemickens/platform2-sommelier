// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_WIMAX_SERVICE_H_
#define SHILL_WIMAX_SERVICE_H_

#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/refptr_types.h"
#include "shill/service.h"
#include "shill/wimax_network_proxy_interface.h"

namespace shill {

class KeyValueStore;

class WiMaxService : public Service {
 public:
  static const char kStorageNetworkId[];

  // TODO(petkov): Declare this in chromeos/dbus/service_constants.h.
  static const char kNetworkIdProperty[];

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
  virtual RpcIdentifier GetNetworkObjectPath() const;

  // Starts the service by associating it with the RPC network object |proxy|
  // and listening for its signal strength. Returns true on success, false
  // otherwise. Takes ownership of proxy, regardless of the result of the
  // operation. The proxy will be destroyed on failure.
  virtual bool Start(WiMaxNetworkProxyInterface *proxy);

  // Stops the service by disassociating it from |proxy_| and resetting its
  // signal strength to 0.
  virtual void Stop();

  virtual bool IsStarted() const;

  const std::string &network_name() const { return network_name_; }
  const WiMaxNetworkId &network_id() const { return network_id_; }
  void set_network_id(const WiMaxNetworkId &id) { network_id_ = id; }

  static WiMaxNetworkId ConvertIdentifierToNetworkId(uint32 identifier);

  // Initializes the storage identifier. Note that the friendly service name and
  // the |network_id_| must already be initialized.
  void InitStorageIdentifier();
  static std::string CreateStorageIdentifier(const WiMaxNetworkId &id,
                                             const std::string &name);

  // Inherited from Service.
  virtual bool TechnologyIs(const Technology::Identifier type) const;
  virtual void Connect(Error *error);
  virtual void Disconnect(Error *error);
  virtual std::string GetStorageIdentifier() const;
  virtual bool Is8021x() const;
  virtual void set_eap(const EapCredentials &eap);
  virtual bool Save(StoreInterface *storage);

 private:
  FRIEND_TEST(WiMaxServiceTest, GetDeviceRpcId);
  FRIEND_TEST(WiMaxServiceTest, OnSignalStrengthChanged);
  FRIEND_TEST(WiMaxServiceTest, SetEAP);
  FRIEND_TEST(WiMaxServiceTest, StartStop);

  virtual std::string GetDeviceRpcId(Error *error);

  void OnSignalStrengthChanged(int strength);

  void UpdateConnectable();

  WiMaxRefPtr wimax_;
  scoped_ptr<WiMaxNetworkProxyInterface> proxy_;
  std::string storage_id_;

  WiMaxNetworkId network_id_;
  std::string network_name_;
  bool need_passphrase_;

  DISALLOW_COPY_AND_ASSIGN(WiMaxService);
};

}  // namespace shill

#endif  // SHILL_WIMAX_SERVICE_H_
