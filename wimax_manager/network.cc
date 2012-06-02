// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wimax_manager/network.h"

#include "wimax_manager/network_dbus_adaptor.h"

using std::string;

namespace wimax_manager {

// static
const int Network::kMaxCINR = 53;
const int Network::kMinCINR = -10;
const int Network::kMaxRSSI = -40;
const int Network::kMinRSSI = -123;

Network::Network(Identifier identifier, const string &name, NetworkType type,
                 int cinr, int rssi)
    : identifier_(identifier), name_(name), type_(type),
      cinr_(cinr), rssi_(rssi) {
}

Network::~Network() {
}

// static
int Network::DecodeCINR(int encoded_cinr) {
  int cinr = encoded_cinr + kMinCINR;
  if (cinr < kMinCINR)
    return kMinCINR;
  if (cinr > kMaxCINR)
    return kMaxCINR;
  return cinr;
}

// static
int Network::DecodeRSSI(int encoded_rssi) {
  int rssi = encoded_rssi + kMinRSSI;
  if (rssi < kMinRSSI)
    return kMinRSSI;
  if (rssi > kMaxRSSI)
    return kMaxRSSI;
  return rssi;
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
