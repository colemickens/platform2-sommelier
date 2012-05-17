// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wimax_manager/network.h"

#include "wimax_manager/network_dbus_adaptor.h"

using std::string;

namespace wimax_manager {

Network::Network(uint32 identifier, const string &name, NetworkType type,
                 int cinr, int rssi)
    : identifier_(identifier), name_(name), type_(type),
      cinr_(cinr), rssi_(rssi) {
}

Network::~Network() {
}

}  // namespace wimax_manager
