// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_WIFI_MOCK_SCAN_SESSION_H_
#define SHILL_WIFI_MOCK_SCAN_SESSION_H_

#include "shill/wifi/scan_session.h"

#include <set>

#include <gmock/gmock.h>

#include "shill/wifi/wifi_provider.h"

namespace shill {

class ByteString;
class EventDispatcher;
class Metrics;
class NetlinkManager;

class MockScanSession : public ScanSession {
 public:
  MockScanSession(NetlinkManager* netlink_manager,
                  EventDispatcher* dispatcher,
                  const WiFiProvider::FrequencyCountList& previous_frequencies,
                  const std::set<uint16_t>& available_frequencies,
                  uint32_t ifindex,
                  const FractionList& fractions,
                  int min_frequencies,
                  int max_frequencies,
                  OnScanFailed on_scan_failed,
                  Metrics* metrics);
  ~MockScanSession() override;

  MOCK_CONST_METHOD0(HasMoreFrequencies, bool());
  MOCK_METHOD1(AddSsid, void(const ByteString& ssid));
  MOCK_METHOD0(InitiateScan, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockScanSession);
};

}  // namespace shill

#endif  // SHILL_WIFI_MOCK_SCAN_SESSION_H_
