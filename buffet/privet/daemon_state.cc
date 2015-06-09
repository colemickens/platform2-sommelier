// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/privet/daemon_state.h"

namespace privetd {

namespace state_key {

const char kDeviceId[] = "id";
const char kDeviceName[] = "name";
const char kDeviceDescription[] = "description";
const char kDeviceLocation[] = "description";

const char kWifiHasBeenBootstrapped[] = "have_ever_been_bootstrapped";
const char kWifiLastConfiguredSSID[] = "last_configured_ssid";

}  // namespace state_key

DaemonState::DaemonState(const base::FilePath& state_path)
    : state_path_(state_path) {
}

void DaemonState::Init() {
  Load(state_path_);
}

void DaemonState::Save() const {
  KeyValueStore::Save(state_path_);
}

}  // namespace privetd
