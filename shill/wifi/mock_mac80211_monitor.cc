// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wifi/mock_mac80211_monitor.h"

namespace shill {

MockMac80211Monitor::MockMac80211Monitor(
    EventDispatcher* dispatcher,
    const std::string& link_name,
    size_t queue_length_limit,
    const base::Closure& on_repair_callback,
    Metrics* metrics)
    : Mac80211Monitor(dispatcher,
                      link_name,
                      queue_length_limit,
                      on_repair_callback,
                      metrics) {}

MockMac80211Monitor::~MockMac80211Monitor() = default;

}  // namespace shill
