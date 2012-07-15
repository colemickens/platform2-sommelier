// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/metrics.h"

#include <chromeos/dbus/service_constants.h>
#include <metrics/metrics_library_mock.h>
#include <metrics/timer_mock.h>

#include "shill/mock_service.h"
#include "shill/mock_wifi_service.h"
#include "shill/property_store_unittest.h"

using testing::_;
using testing::DoAll;
using testing::Ge;
using testing::Mock;
using testing::Return;
using testing::SetArgumentPointee;
using testing::Test;

namespace shill {

class MetricsTest : public PropertyStoreTest {
 public:
  MetricsTest()
      : service_(new MockService(control_interface(),
                                 dispatcher(),
                                 &metrics_,
                                 manager())),
        wifi_(new WiFi(control_interface(),
                       dispatcher(),
                       &metrics_,
                       manager(),
                       "wlan0",
                       "000102030405",
                       0)),
        wifi_service_(new MockWiFiService(control_interface(),
                                          dispatcher(),
                                          &metrics_,
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
    metrics_.collect_bootstats_ = false;
  }

 protected:
  void ExpectCommonPostReady(Metrics::WiFiChannel channel,
                             Metrics::WiFiNetworkPhyMode mode,
                             Metrics::WiFiSecurity security) {
    EXPECT_CALL(library_, SendEnumToUMA("Network.Shill.Wifi.Channel",
                                        channel,
                                        Metrics::kMetricNetworkChannelMax));
    EXPECT_CALL(library_, SendEnumToUMA("Network.Shill.Wifi.PhyMode",
                                        mode,
                                        Metrics::kWiFiNetworkPhyModeMax));
    EXPECT_CALL(library_, SendEnumToUMA("Network.Shill.Wifi.Security",
                                        security,
                                        Metrics::kWiFiSecurityMax));
  }

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
                                  Metrics::kTimerHistogramMillisecondsMin,
                                  Metrics::kTimerHistogramMillisecondsMax,
                                  Metrics::kTimerHistogramNumBuckets));
  metrics_.RegisterService(service_);
  metrics_.NotifyServiceStateChanged(service_, Service::kStateConfiguring);
  metrics_.NotifyServiceStateChanged(service_, Service::kStateConnected);
}

TEST_F(MetricsTest, TimeToPortal) {
  EXPECT_CALL(library_, SendToUMA("Network.Shill.Unknown.TimeToPortal",
                                  Ge(0),
                                  Metrics::kTimerHistogramMillisecondsMin,
                                  Metrics::kTimerHistogramMillisecondsMax,
                                  Metrics::kTimerHistogramNumBuckets));
  metrics_.RegisterService(service_);
  metrics_.NotifyServiceStateChanged(service_, Service::kStateConnected);
  metrics_.NotifyServiceStateChanged(service_, Service::kStatePortal);
}

TEST_F(MetricsTest, TimeToOnline) {
  EXPECT_CALL(library_, SendToUMA("Network.Shill.Unknown.TimeToOnline",
                                  Ge(0),
                                  Metrics::kTimerHistogramMillisecondsMin,
                                  Metrics::kTimerHistogramMillisecondsMax,
                                  Metrics::kTimerHistogramNumBuckets));
  metrics_.RegisterService(service_);
  metrics_.NotifyServiceStateChanged(service_, Service::kStateConnected);
  metrics_.NotifyServiceStateChanged(service_, Service::kStateOnline);
}

TEST_F(MetricsTest, ServiceFailure) {
  EXPECT_CALL(*service_.get(), failure())
      .WillRepeatedly(Return(Service::kFailureBadPassphrase));
  EXPECT_CALL(library_, SendEnumToUMA(Metrics::kMetricNetworkServiceErrors,
                                      Service::kFailureBadPassphrase,
                                      Metrics::kMetricNetworkServiceErrorsMax));
  metrics_.RegisterService(service_);
  metrics_.NotifyServiceStateChanged(service_, Service::kStateFailure);
}

TEST_F(MetricsTest, WiFiServiceTimeToJoin) {
  EXPECT_CALL(library_, SendToUMA("Network.Shill.Wifi.TimeToJoin",
                                  Ge(0),
                                  Metrics::kTimerHistogramMillisecondsMin,
                                  Metrics::kTimerHistogramMillisecondsMax,
                                  Metrics::kTimerHistogramNumBuckets));
  metrics_.RegisterService(wifi_service_);
  metrics_.NotifyServiceStateChanged(wifi_service_, Service::kStateAssociating);
  metrics_.NotifyServiceStateChanged(wifi_service_, Service::kStateConfiguring);
}

TEST_F(MetricsTest, WiFiServicePostReady) {
  base::TimeDelta non_zero_time_delta = base::TimeDelta::FromMilliseconds(1);
  chromeos_metrics::TimerMock *mock_time_resume_to_ready_timer =
      new chromeos_metrics::TimerMock;
  metrics_.set_time_resume_to_ready_timer(mock_time_resume_to_ready_timer);

  ExpectCommonPostReady(Metrics::kWiFiChannel2412,
                        Metrics::kWiFiNetworkPhyMode11a,
                        Metrics::kWiFiSecurityWep);
  EXPECT_CALL(library_, SendToUMA("Network.Shill.Wifi.TimeResumeToReady",
                                  _, _, _, _)).Times(0);
  wifi_service_->frequency_ = 2412;
  wifi_service_->physical_mode_ = Metrics::kWiFiNetworkPhyMode11a;
  wifi_service_->security_ = flimflam::kSecurityWep;
  metrics_.RegisterService(wifi_service_);
  metrics_.NotifyServiceStateChanged(wifi_service_, Service::kStateConnected);
  Mock::VerifyAndClearExpectations(&library_);

  // Simulate a system suspend, resume and an AP reconnect.
  ExpectCommonPostReady(Metrics::kWiFiChannel2412,
                        Metrics::kWiFiNetworkPhyMode11a,
                        Metrics::kWiFiSecurityWep);
  EXPECT_CALL(library_, SendToUMA("Network.Shill.Wifi.TimeResumeToReady",
                                  Ge(0),
                                  Metrics::kTimerHistogramMillisecondsMin,
                                  Metrics::kTimerHistogramMillisecondsMax,
                                  Metrics::kTimerHistogramNumBuckets));
  EXPECT_CALL(*mock_time_resume_to_ready_timer, GetElapsedTime(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(non_zero_time_delta), Return(true)));
  metrics_.NotifyPowerStateChange(PowerManagerProxyDelegate::kMem);
  metrics_.NotifyPowerStateChange(PowerManagerProxyDelegate::kOn);
  metrics_.NotifyServiceStateChanged(wifi_service_, Service::kStateConnected);
  Mock::VerifyAndClearExpectations(&library_);
  Mock::VerifyAndClearExpectations(mock_time_resume_to_ready_timer);

  // Make sure subsequent connects do not count towards TimeResumeToReady.
  ExpectCommonPostReady(Metrics::kWiFiChannel2412,
                        Metrics::kWiFiNetworkPhyMode11a,
                        Metrics::kWiFiSecurityWep);
  EXPECT_CALL(library_, SendToUMA("Network.Shill.Wifi.TimeResumeToReady",
                                  _, _, _, _)).Times(0);
  metrics_.NotifyServiceStateChanged(wifi_service_, Service::kStateConnected);
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

TEST_F(MetricsTest, TimeOnlineTimeToDrop) {
  chromeos_metrics::TimerMock *mock_time_online_timer =
      new chromeos_metrics::TimerMock;
  metrics_.set_time_online_timer(mock_time_online_timer);
  chromeos_metrics::TimerMock *mock_time_to_drop_timer =
      new chromeos_metrics::TimerMock;
  metrics_.set_time_to_drop_timer(mock_time_to_drop_timer);
  EXPECT_CALL(*service_.get(), technology()).
      WillOnce(Return(Technology::kEthernet));
  EXPECT_CALL(library_, SendToUMA("Network.Shill.Ethernet.TimeOnline",
                                  Ge(0),
                                  Metrics::kMetricTimeOnlineSecondsMin,
                                  Metrics::kMetricTimeOnlineSecondsMax,
                                  Metrics::kTimerHistogramNumBuckets));
  EXPECT_CALL(library_, SendToUMA(Metrics::kMetricTimeToDropSeconds,
                                  Ge(0),
                                  Metrics::kMetricTimeToDropSecondsMin,
                                  Metrics::kMetricTimeToDropSecondsMax,
                                  Metrics::kTimerHistogramNumBuckets)).Times(0);
  EXPECT_CALL(*mock_time_online_timer, Start()).Times(2);
  EXPECT_CALL(*mock_time_to_drop_timer, Start());
  metrics_.NotifyDefaultServiceChanged(service_);
  metrics_.NotifyDefaultServiceChanged(wifi_service_);

  EXPECT_CALL(*mock_time_online_timer, Start());
  EXPECT_CALL(*mock_time_to_drop_timer, Start()).Times(0);
  EXPECT_CALL(library_, SendToUMA("Network.Shill.Wifi.TimeOnline",
                                  Ge(0),
                                  Metrics::kMetricTimeOnlineSecondsMin,
                                  Metrics::kMetricTimeOnlineSecondsMax,
                                  Metrics::kTimerHistogramNumBuckets));
  EXPECT_CALL(library_, SendToUMA(Metrics::kMetricTimeToDropSeconds,
                                  Ge(0),
                                  Metrics::kMetricTimeToDropSecondsMin,
                                  Metrics::kMetricTimeToDropSecondsMax,
                                  Metrics::kTimerHistogramNumBuckets));
  metrics_.NotifyDefaultServiceChanged(NULL);
}

TEST_F(MetricsTest, Disconnect) {
  EXPECT_CALL(*service_.get(), technology()).
      WillRepeatedly(Return(Technology::kWifi));
  EXPECT_CALL(*service_.get(), explicitly_disconnected()).
      WillOnce(Return(false));
  EXPECT_CALL(library_, SendToUMA("Network.Shill.Wifi.Disconnect",
                                  false,
                                  Metrics::kMetricDisconnectMin,
                                  Metrics::kMetricDisconnectMax,
                                  Metrics::kMetricDisconnectNumBuckets));
  metrics_.NotifyServiceDisconnect(service_);

  EXPECT_CALL(*service_.get(), explicitly_disconnected()).
      WillOnce(Return(true));
  EXPECT_CALL(library_, SendToUMA("Network.Shill.Wifi.Disconnect",
                                  true,
                                  Metrics::kMetricDisconnectMin,
                                  Metrics::kMetricDisconnectMax,
                                  Metrics::kMetricDisconnectNumBuckets));
  metrics_.NotifyServiceDisconnect(service_);
}

TEST_F(MetricsTest, PortalDetectionResultToEnum) {
  PortalDetector::Result result(PortalDetector::kPhaseDNS,
                                PortalDetector::kStatusFailure, 0, true);
  EXPECT_EQ(Metrics::kPortalResultDNSFailure,
            Metrics::PortalDetectionResultToEnum(result));

  result.phase = PortalDetector::kPhaseDNS;
  result.status = PortalDetector::kStatusTimeout;
  EXPECT_EQ(Metrics::kPortalResultDNSTimeout,
            Metrics::PortalDetectionResultToEnum(result));

  result.phase = PortalDetector::kPhaseConnection;
  result.status = PortalDetector::kStatusFailure;
  EXPECT_EQ(Metrics::kPortalResultConnectionFailure,
            Metrics::PortalDetectionResultToEnum(result));

  result.phase = PortalDetector::kPhaseConnection;
  result.status = PortalDetector::kStatusTimeout;
  EXPECT_EQ(Metrics::kPortalResultConnectionTimeout,
            Metrics::PortalDetectionResultToEnum(result));

  result.phase = PortalDetector::kPhaseHTTP;
  result.status = PortalDetector::kStatusFailure;
  EXPECT_EQ(Metrics::kPortalResultHTTPFailure,
            Metrics::PortalDetectionResultToEnum(result));

  result.phase = PortalDetector::kPhaseHTTP;
  result.status = PortalDetector::kStatusTimeout;
  EXPECT_EQ(Metrics::kPortalResultHTTPTimeout,
            Metrics::PortalDetectionResultToEnum(result));

  result.phase = PortalDetector::kPhaseContent;
  result.status = PortalDetector::kStatusSuccess;
  EXPECT_EQ(Metrics::kPortalResultSuccess,
            Metrics::PortalDetectionResultToEnum(result));

  result.phase = PortalDetector::kPhaseContent;
  result.status = PortalDetector::kStatusFailure;
  EXPECT_EQ(Metrics::kPortalResultContentFailure,
            Metrics::PortalDetectionResultToEnum(result));

  result.phase = PortalDetector::kPhaseContent;
  result.status = PortalDetector::kStatusTimeout;
  EXPECT_EQ(Metrics::kPortalResultContentTimeout,
            Metrics::PortalDetectionResultToEnum(result));

  result.phase = PortalDetector::kPhaseUnknown;
  result.status = PortalDetector::kStatusFailure;
  EXPECT_EQ(Metrics::kPortalResultUnknown,
            Metrics::PortalDetectionResultToEnum(result));
}

#ifndef NDEBUG

typedef MetricsTest MetricsDeathTest;

TEST_F(MetricsDeathTest, PortalDetectionResultToEnumDNSSuccess) {
  PortalDetector::Result result(PortalDetector::kPhaseDNS,
                                PortalDetector::kStatusSuccess, 0, true);
  EXPECT_DEATH(Metrics::PortalDetectionResultToEnum(result),
               "Final result status 1 is not allowed in the DNS phase");
}

TEST_F(MetricsDeathTest, PortalDetectionResultToEnumConnectionSuccess) {
  PortalDetector::Result result(PortalDetector::kPhaseConnection,
                                PortalDetector::kStatusSuccess, 0, true);
  EXPECT_DEATH(Metrics::PortalDetectionResultToEnum(result),
               "Final result status 1 is not allowed in the Connection phase");
}

TEST_F(MetricsDeathTest, PortalDetectionResultToEnumHTTPSuccess) {
  PortalDetector::Result result(PortalDetector::kPhaseHTTP,
                                PortalDetector::kStatusSuccess, 0, true);
  EXPECT_DEATH(Metrics::PortalDetectionResultToEnum(result),
               "Final result status 1 is not allowed in the HTTP phase");
}

#endif  // NDEBUG

}  // namespace shill
