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
const Network::Identifier Network::kInvalidIdentifier = 0;

Network::Network(Identifier identifier, const string &name, NetworkType type,
                 int cinr, int rssi)
    : identifier_(identifier),
      name_(name),
      type_(type),
      cinr_(cinr),
      rssi_(rssi) {
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
  // TODO(benchan): Derive a RSSI/CINR to signal strength mapping equation
  // with finer granularity.

  // RSSI \ CINR | [-10..-3] | (-3..0] | (0..3] | (3.10] | (10..15] | (15..53]
  // ------------+-----------+---------+--------+--------+----------+---------
  // [-123..-80] |     0     |    0    |    0   |    0   |     0    |     0
  // ( -80..-75] |     0     |    0    |    0   |   20   |    20    |    40
  // ( -75..-65] |     0     |    0    |   20   |   20   |    40    |    60
  // ( -65..-55] |     0     |   20    |   20   |   40   |    60    |    80
  // ( -55..-40] |     0     |   20    |   40   |   60   |    80    |   100
  static const int kSignalStrengthTable[5][6] = {
    { 0,  0,  0,  0,  0,   0 },
    { 0,  0,  0, 20, 20,  40 },
    { 0,  0, 20, 20, 40,  60 },
    { 0, 20, 20, 40, 60,  80 },
    { 0, 20, 40, 60, 80, 100 },
  };

  int row = 4;
  if (rssi_ <= -80) {
    row = 0;
  } else if (rssi_ <= -75) {
    row = 1;
  } else if (rssi_ <= -65) {
    row = 2;
  } else if (rssi_ <= -55) {
    row = 3;
  }

  int column = 5;
  if (cinr_ <= -3) {
    column = 0;
  } else if (cinr_ <= 0) {
    column = 1;
  } else if (cinr_ <= 3) {
    column = 2;
  } else if (cinr_ <= 10) {
    column = 3;
  } else if (cinr_ <= 15) {
    column = 4;
  }

  return kSignalStrengthTable[row][column];
}

}  // namespace wimax_manager
