// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_WIFI_MOCK_WAKE_ON_WIFI_H_
#define SHILL_WIFI_MOCK_WAKE_ON_WIFI_H_

#include <string>
#include <vector>

#include <gmock/gmock.h>

#include "shill/error.h"
#include "shill/net/nl80211_message.h"
#include "shill/property_store.h"
#include "shill/wifi/wake_on_wifi_interface.h"

namespace shill {

class MockWakeOnWiFi : public WakeOnWiFiInterface {
 public:
  MockWakeOnWiFi();
  ~MockWakeOnWiFi() override;

  MOCK_METHOD1(InitPropertyStore, void(PropertyStore* store));
  MOCK_METHOD0(StartMetricsTimer, void());
  MOCK_METHOD2(AddWakeOnPacketConnection,
               void(const std::string& ip_endpoint, Error* error));
  MOCK_METHOD2(AddWakeOnPacketOfTypes,
               void(const std::vector<std::string>& packet_types,
                    Error* error));
  MOCK_METHOD2(RemoveWakeOnPacketConnection,
               void(const std::string& ip_endpoint, Error* error));
  MOCK_METHOD2(RemoveWakeOnPacketOfTypes,
               void(const std::vector<std::string>& packet_types,
                    Error* error));
  MOCK_METHOD1(RemoveAllWakeOnPacketConnections, void(Error* error));
  MOCK_METHOD1(ParseWakeOnWiFiCapabilities,
               void(const Nl80211Message& nl80211_message));
  MOCK_METHOD7(OnBeforeSuspend,
               void(bool is_connected,
                    const std::vector<ByteString>& ssid_whitelist,
                    const ResultCallback& done_callback,
                    const base::Closure& renew_dhcp_lease_callback,
                    const base::Closure& remove_supplicant_networks_callback,
                    bool have_dhcp_lease,
                    uint32_t time_to_next_lease_renewal));
  MOCK_METHOD0(OnAfterResume, void());
  MOCK_METHOD6(OnDarkResume,
               void(bool is_connected,
                    const std::vector<ByteString>& ssid_whitelist,
                    const ResultCallback& done_callback,
                    const base::Closure& renew_dhcp_lease_callback,
                    const InitiateScanCallback& initiate_scan_callback,
                    const base::Closure& remove_supplicant_networks_callback));
  MOCK_METHOD2(OnConnectedAndReachable,
               void(bool start_lease_renewal_timer,
                    uint32_t time_to_next_lease_renewal));
  MOCK_METHOD2(ReportConnectedToServiceAfterWake,
               void(bool is_connected, int seconds_in_suspend));
  MOCK_METHOD3(OnNoAutoConnectableServicesAfterScan,
               void(const std::vector<ByteString>& ssid_whitelist,
                    const base::Closure& remove_supplicant_networks_callback,
                    const InitiateScanCallback& initiate_scan_callback));
  MOCK_METHOD1(OnScanStarted, void(bool is_active_scan));
  MOCK_METHOD0(InDarkResume, bool());
  MOCK_METHOD1(OnWiphyIndexReceived, void(uint32_t));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockWakeOnWiFi);
};

}  // namespace shill

#endif  // SHILL_WIFI_MOCK_WAKE_ON_WIFI_H_
