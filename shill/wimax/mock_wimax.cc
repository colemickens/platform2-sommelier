// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wimax/mock_wimax.h"

#include <string>

namespace shill {

using std::string;

MockWiMax::MockWiMax(ControlInterface* control,
                     EventDispatcher* dispatcher,
                     Metrics* metrics,
                     Manager* manager,
                     const string& link_name,
                     const string& address,
                     int interface_index,
                     const RpcIdentifier& path)
    : WiMax(control, dispatcher, metrics, manager, link_name, address,
            interface_index, path) {
}

MockWiMax::~MockWiMax() {}

}  // namespace shill
