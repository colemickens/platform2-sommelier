// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mock_wimax.h"

#include <string>

namespace shill {

using std::string;

MockWiMax::MockWiMax(ControlInterface *control,
                     EventDispatcher *dispatcher,
                     Metrics *metrics,
                     Manager *manager,
                     const string &link_name,
                     int interface_index)
    : WiMax(control, dispatcher, metrics, manager, link_name, interface_index) {
}

MockWiMax::~MockWiMax() {}

}  // namespace shill
