// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wimax_manager/network_dbus_adaptor.h"

#include <base/logging.h>
#include <base/stringprintf.h>
#include <chromeos/dbus/service_constants.h>

#include "wimax_manager/network.h"

using org::chromium::WiMaxManager::Network_adaptor;
using std::string;

namespace wimax_manager {

NetworkDBusAdaptor::NetworkDBusAdaptor(DBus::Connection *connection,
                                       Network *network)
    : DBusAdaptor(connection, GetNetworkObjectPath(*network)),
      network_(network) {
  Identifier = network->identifier();
  Name = network->name();
  Type = network->type();
  CINR = network->cinr();
  RSSI = network->rssi();
}

NetworkDBusAdaptor::~NetworkDBusAdaptor() {
}

// static
string NetworkDBusAdaptor::GetNetworkObjectPath(const Network &network) {
  return base::StringPrintf("%s%08x", kNetworkObjectPathPrefix,
                            network.identifier());
}

}  // namespace wimax_manager
