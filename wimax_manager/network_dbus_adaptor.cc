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
  // Initialize |SignalStrength| before UpdateProperties() reads it for
  // the first time.
  SignalStrength = network_->GetSignalStrength();
  UpdateProperties();
}

NetworkDBusAdaptor::~NetworkDBusAdaptor() {
}

// static
string NetworkDBusAdaptor::GetNetworkObjectPath(const Network &network) {
  return base::StringPrintf("%s%08x", kNetworkObjectPathPrefix,
                            network.identifier());
}

void NetworkDBusAdaptor::UpdateProperties() {
  Identifier = network_->identifier();
  Name = network_->name();
  Type = network_->type();
  CINR = network_->cinr();
  RSSI = network_->rssi();
  int signal_strength = network_->GetSignalStrength();
  if (SignalStrength() != signal_strength) {
    SignalStrength = signal_strength;
    SignalStrengthChanged(signal_strength);
  }
}

}  // namespace wimax_manager
