// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIMAX_MANAGER_NETWORK_DBUS_ADAPTOR_H_
#define WIMAX_MANAGER_NETWORK_DBUS_ADAPTOR_H_

#include <string>

#include "wimax_manager/dbus_adaptor.h"
#include "wimax_manager/dbus_bindings/network.h"

namespace wimax_manager {

class Network;

class NetworkDBusAdaptor : public org::chromium::WiMaxManager::Network_adaptor,
                           public DBusAdaptor {
 public:
  NetworkDBusAdaptor(DBus::Connection *connection, Network *network);
  virtual ~NetworkDBusAdaptor();

  static std::string GetNetworkObjectPath(const Network &network);

  void UpdateProperties();

 private:
  Network *network_;

  DISALLOW_COPY_AND_ASSIGN(NetworkDBusAdaptor);
};

}  // namespace wimax_manager

#endif  // WIMAX_MANAGER_NETWORK_DBUS_ADAPTOR_H_
