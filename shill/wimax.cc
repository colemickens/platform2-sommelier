// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wimax.h"

using std::string;

namespace shill {

WiMax::WiMax(ControlInterface *control,
             EventDispatcher *dispatcher,
             Metrics *metrics,
             Manager *manager,
             const string &link_name,
             int interface_index)
    : Device(control, dispatcher, metrics, manager, link_name, "",
             interface_index, Technology::kWiMax) {}

WiMax::~WiMax() {}

void WiMax::Start(Error *error, const EnabledStateChangedCallback &callback) {
}

void WiMax::Stop(Error *error, const EnabledStateChangedCallback &callback) {
}

bool WiMax::TechnologyIs(const Technology::Identifier type) const {
  return type == Technology::kWiMax;
}

void WiMax::Connect(Error *error) {
}

void WiMax::Disconnect(Error *error) {
}

}  // namespace shill
