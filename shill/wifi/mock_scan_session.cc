// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wifi/mock_scan_session.h"

#include <gmock/gmock.h>

#include "shill/wifi/wifi_provider.h"

namespace shill {

MockScanSession::MockScanSession(NetlinkManager* netlink_manager,
                                 EventDispatcher* dispatcher,
                                 const WiFiProvider::FrequencyCountList
                                     &previous_frequencies,
                                 const std::set<uint16_t>
                                     &available_frequencies,
                                 uint32_t ifindex,
                                 const FractionList& fractions,
                                 int min_frequencies,
                                 int max_frequencies,
                                 OnScanFailed on_scan_failed,
                                 Metrics* metrics)
    : ScanSession(netlink_manager,
                  dispatcher,
                  previous_frequencies,
                  available_frequencies,
                  ifindex,
                  fractions,
                  min_frequencies,
                  max_frequencies,
                  on_scan_failed,
                  metrics) {
  ON_CALL(*this, HasMoreFrequencies()).WillByDefault(testing::Return(true));
}

MockScanSession::~MockScanSession() {}


}  // namespace shill
