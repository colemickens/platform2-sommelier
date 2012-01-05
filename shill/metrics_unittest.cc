// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/metrics.h"

#include <chromeos/dbus/service_constants.h>
#include <metrics/metrics_library_mock.h>
#include <metrics/timer.h>

#include "shill/mock_service.h"
#include "shill/mock_wifi_service.h"
#include "shill/property_store_unittest.h"

using testing::_;
using testing::Ge;
using testing::Return;
using testing::Test;

namespace shill {

class MetricsTest : public PropertyStoreTest {
 public:
  MetricsTest()
      : service_(new MockService(control_interface(),
                                 dispatcher(),
                                 manager())),
        wifi_(new WiFi(control_interface(),
                       dispatcher(),
                       manager(),
                       "wlan0",
                       "000102030405",
                       0)),
        wifi_service_(new MockWiFiService(control_interface(),
                                          dispatcher(),
                                          manager(),
                                          wifi_,
                                          ssid_,
                                          flimflam::kModeManaged,
                                          flimflam::kSecurityNone,
                                          false)) {
  }

  virtual ~MetricsTest() {}

  virtual void SetUp() {
    metrics_.set_library(&library_);
    service_->set_metrics(&metrics_);
    wifi_service_->set_metrics(&metrics_);
  }

 protected:
  Metrics metrics_;  // This must be destroyed after service_ and wifi_service_
  MetricsLibraryMock library_;
  scoped_refptr<MockService> service_;
  WiFiRefPtr wifi_;
  const std::vector<uint8_t> ssid_;
  scoped_refptr<MockWiFiService> wifi_service_;
};

TEST_F(MetricsTest, TimeToConfig) {
  EXPECT_CALL(library_, SendToUMA("Network.Shill.Unknown.TimeToConfig",
                                  Ge(0),
                                  Metrics::kTimerHistogramMinMilliseconds,
                                  Metrics::kTimerHistogramMaxMilliseconds,
                                  Metrics::kTimerHistogramNumBuckets));
  metrics_.RegisterService(service_);
  metrics_.NotifyServiceStateChanged(service_, Service::kStateConfiguring);
  metrics_.NotifyServiceStateChanged(service_, Service::kStateReady);
}

TEST_F(MetricsTest, TimeToPortal) {
  EXPECT_CALL(library_, SendToUMA("Network.Shill.Unknown.TimeToPortal",
                                  Ge(0),
                                  Metrics::kTimerHistogramMinMilliseconds,
                                  Metrics::kTimerHistogramMaxMilliseconds,
                                  Metrics::kTimerHistogramNumBuckets));
  metrics_.RegisterService(service_);
  metrics_.NotifyServiceStateChanged(service_, Service::kStateReady);
  metrics_.NotifyServiceStateChanged(service_, Service::kStatePortal);
}

TEST_F(MetricsTest, TimeToOnline) {
  EXPECT_CALL(library_, SendToUMA("Network.Shill.Unknown.TimeToOnline",
                                  Ge(0),
                                  Metrics::kTimerHistogramMinMilliseconds,
                                  Metrics::kTimerHistogramMaxMilliseconds,
                                  Metrics::kTimerHistogramNumBuckets));
  metrics_.RegisterService(service_);
  metrics_.NotifyServiceStateChanged(service_, Service::kStateReady);
  metrics_.NotifyServiceStateChanged(service_, Service::kStateOnline);
}

TEST_F(MetricsTest, ServiceFailure) {
  EXPECT_CALL(*service_.get(), failure())
      .WillRepeatedly(Return(Service::kFailureBadCredentials));
  EXPECT_CALL(library_, SendEnumToUMA(Metrics::kMetricNetworkServiceErrors,
                                      Service::kFailureBadCredentials,
                                      Metrics::kMetricNetworkServiceErrorsMax));
  metrics_.RegisterService(service_);
  metrics_.NotifyServiceStateChanged(service_, Service::kStateFailure);
}

TEST_F(MetricsTest, WiFiServiceTimeToJoin) {
  EXPECT_CALL(library_, SendToUMA("Network.Shill.Wifi.TimeToJoin",
                                  Ge(0),
                                  Metrics::kTimerHistogramMinMilliseconds,
                                  Metrics::kTimerHistogramMaxMilliseconds,
                                  Metrics::kTimerHistogramNumBuckets));
  metrics_.RegisterService(wifi_service_);
  metrics_.NotifyServiceStateChanged(wifi_service_, Service::kStateAssociating);
  metrics_.NotifyServiceStateChanged(wifi_service_, Service::kStateConfiguring);
}

TEST_F(MetricsTest, WiFiServicePostReady) {
  EXPECT_CALL(library_, SendEnumToUMA("Network.Shill.Wifi.Channel",
                                      Metrics::kWiFiChannel2412,
                                      Metrics::kMetricNetworkChannelMax));
  EXPECT_CALL(library_, SendEnumToUMA("Network.Shill.Wifi.PhyMode",
                                      Metrics::kWiFiNetworkPhyMode11a,
                                      Metrics::kWiFiNetworkPhyModeMax));
  EXPECT_CALL(library_, SendEnumToUMA("Network.Shill.Wifi.Security",
                                      Metrics::kWiFiSecurityWep,
                                      Metrics::kWiFiSecurityMax));
  wifi_service_->frequency_ = 2412;
  wifi_service_->physical_mode_ = Metrics::kWiFiNetworkPhyMode11a;
  wifi_service_->security_ = flimflam::kSecurityWep;
  metrics_.RegisterService(wifi_service_);
  metrics_.NotifyServiceStateChanged(wifi_service_, Service::kStateReady);
}

TEST_F(MetricsTest, FrequencyToChannel) {
  EXPECT_EQ(Metrics::kWiFiChannelUndef, metrics_.WiFiFrequencyToChannel(2411));
  EXPECT_EQ(Metrics::kWiFiChannel2412, metrics_.WiFiFrequencyToChannel(2412));
  EXPECT_EQ(Metrics::kWiFiChannel2472, metrics_.WiFiFrequencyToChannel(2472));
  EXPECT_EQ(Metrics::kWiFiChannelUndef, metrics_.WiFiFrequencyToChannel(2473));
  EXPECT_EQ(Metrics::kWiFiChannel2484, metrics_.WiFiFrequencyToChannel(2484));
  EXPECT_EQ(Metrics::kWiFiChannelUndef, metrics_.WiFiFrequencyToChannel(5169));
  EXPECT_EQ(Metrics::kWiFiChannel5170, metrics_.WiFiFrequencyToChannel(5170));
  EXPECT_EQ(Metrics::kWiFiChannel5190, metrics_.WiFiFrequencyToChannel(5190));
  EXPECT_EQ(Metrics::kWiFiChannel5180, metrics_.WiFiFrequencyToChannel(5180));
  EXPECT_EQ(Metrics::kWiFiChannel5200, metrics_.WiFiFrequencyToChannel(5200));
  EXPECT_EQ(Metrics::kWiFiChannel5230, metrics_.WiFiFrequencyToChannel(5230));
  EXPECT_EQ(Metrics::kWiFiChannelUndef, metrics_.WiFiFrequencyToChannel(5231));
  EXPECT_EQ(Metrics::kWiFiChannelUndef, metrics_.WiFiFrequencyToChannel(5239));
  EXPECT_EQ(Metrics::kWiFiChannel5240, metrics_.WiFiFrequencyToChannel(5240));
  EXPECT_EQ(Metrics::kWiFiChannelUndef, metrics_.WiFiFrequencyToChannel(5241));
  EXPECT_EQ(Metrics::kWiFiChannel5320, metrics_.WiFiFrequencyToChannel(5320));
  EXPECT_EQ(Metrics::kWiFiChannelUndef, metrics_.WiFiFrequencyToChannel(5321));
  EXPECT_EQ(Metrics::kWiFiChannelUndef, metrics_.WiFiFrequencyToChannel(5499));
  EXPECT_EQ(Metrics::kWiFiChannel5500, metrics_.WiFiFrequencyToChannel(5500));
  EXPECT_EQ(Metrics::kWiFiChannelUndef, metrics_.WiFiFrequencyToChannel(5501));
  EXPECT_EQ(Metrics::kWiFiChannel5700, metrics_.WiFiFrequencyToChannel(5700));
  EXPECT_EQ(Metrics::kWiFiChannelUndef, metrics_.WiFiFrequencyToChannel(5701));
  EXPECT_EQ(Metrics::kWiFiChannelUndef, metrics_.WiFiFrequencyToChannel(5744));
  EXPECT_EQ(Metrics::kWiFiChannel5745, metrics_.WiFiFrequencyToChannel(5745));
  EXPECT_EQ(Metrics::kWiFiChannelUndef, metrics_.WiFiFrequencyToChannel(5746));
  EXPECT_EQ(Metrics::kWiFiChannel5825, metrics_.WiFiFrequencyToChannel(5825));
  EXPECT_EQ(Metrics::kWiFiChannelUndef, metrics_.WiFiFrequencyToChannel(5826));
}

}  // namespace shill
