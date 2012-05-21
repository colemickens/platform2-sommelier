// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wimax_manager/network.h"

#include "wimax_manager/network_dbus_adaptor.h"

using std::string;

namespace wimax_manager {

Network::Network(Identifier identifier, const string &name, NetworkType type,
                 int cinr, int rssi)
    : identifier_(identifier), name_(name), type_(type),
      cinr_(cinr), rssi_(rssi) {
}

Network::~Network() {
}

void Network::UpdateFrom(const Network &network) {
  identifier_ = network.identifier_;
  name_ = network.name_;
  type_ = network.type_;
  cinr_ = network.cinr_;
  rssi_ = network.rssi_;

  if (dbus_adaptor())
    dbus_adaptor()->UpdateProperties();
}

int Network::GetSignalStrength() const {
  // According to IEEE 802.16, RSSI should be ranging from -123 to -40 dBm
  // with 1 dBm increment.
  int rssi = rssi_;
  if (rssi < kMinRSSI)
    rssi = kMinRSSI;
  if (rssi > kMaxRSSI)
    rssi = kMaxRSSI;

  // Mapping from [-123, -40] to [0, 100] using integer divison.
  int range_size = kMaxRSSI - kMinRSSI;
  int half_range_size = range_size / 2;
  int offset = rssi - kMinRSSI;
  return (offset * 100 + half_range_size) / range_size;
}

}  // namespace wimax_manager
