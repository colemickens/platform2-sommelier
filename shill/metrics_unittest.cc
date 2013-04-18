// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/metrics.h"

#include <string>

#include <chromeos/dbus/service_constants.h>
#include <metrics/metrics_library_mock.h>
#include <metrics/timer_mock.h>

#include "shill/mock_control.h"
#include "shill/mock_eap_credentials.h"
#include "shill/mock_event_dispatcher.h"
#include "shill/mock_glib.h"
#include "shill/mock_manager.h"
#include "shill/mock_service.h"
#include "shill/mock_wifi_service.h"

using std::string;

using testing::_;
using testing::DoAll;
using testing::Ge;
using testing::Mock;
using testing::Return;
using testing::SetArgumentPointee;
using testing::Test;

namespace shill {

class MetricsTest : public Test {
 public:
  MetricsTest()
      : manager_(&control_interface_,
                 &dispatcher_,
                 &metrics_,
                 &glib_),
        metrics_(&dispatcher_),
        service_(new MockService(&control_interface_,
                                 &dispatcher_,
                                 &metrics_,
                                 &manager_)),
        open_wifi_service_(new MockWiFiService(&control_interface_,
                                               &dispatcher_,
                                               &metrics_,
                                               &manager_,
                                               manager_.wifi_provider(),
                                               ssid_,
                                               flimflam::kModeManaged,
                                               flimflam::kSecurityNone,
                                               false)),
        wep_wifi_service_(new MockWiFiService(&control_interface_,
                                              &dispatcher_,
                                              &metrics_,
                                              &manager_,
                                              manager_.wifi_provider(),
                                              ssid_,
                                              flimflam::kModeManaged,
                                              flimflam::kSecurityWep,
                                              false)),
        eap_wifi_service_(new MockWiFiService(&control_interface_,
                                              &dispatcher_,
                                              &metrics_,
                                              &manager_,
                                              manager_.wifi_provider(),
                                              ssid_,
                                              flimflam::kModeManaged,
                                              flimflam::kSecurity8021x,
                                              false)),
        eap_(new MockEapCredentials()) {}

  virtual ~MetricsTest() {}

  virtual void SetUp() {
    metrics_.set_library(&library_);
    eap_wifi_service_->eap_.reset(eap_);  // Passes ownership.
    metrics_.collect_bootstats_ = false;
  }

 protected:
  void ExpectCommonPostReady(Metrics::WiFiChannel channel,
                             Metrics::WiFiNetworkPhyMode mode,
                             Metrics::WiFiSecurity security,
                             int signal_strength) {
    EXPECT_CALL(library_, SendEnumToUMA("Network.Shill.Wifi.Channel",
                                        channel,
                                        Metrics::kMetricNetworkChannelMax));
    EXPECT_CALL(library_, SendEnumToUMA("Network.Shill.Wifi.PhyMode",
                                        mode,
                                        Metrics::kWiFiNetworkPhyModeMax));
    EXPECT_CALL(library_, SendEnumToUMA("Network.Shill.Wifi.Security",
                                        security,
                                        Metrics::kWiFiSecurityMax));
    EXPECT_CALL(library_,
                SendToUMA("Network.Shill.Wifi.SignalStrength",
                          signal_strength,
                          Metrics::kMetricNetworkSignalStrengthMin,
                          Metrics::kMetricNetworkSignalStrengthMax,
                          Metrics::kMetricNetworkSignalStrengthNumBuckets));
  }

  MockControl control_interface_;
  MockEventDispatcher dispatcher_;
  MockGLib glib_;
  MockManager manager_;
  Metrics metrics_;  // This must be destroyed after all |service_|s.
  MetricsLibraryMock library_;
  scoped_refptr<MockService> service_;
  const std::vector<uint8_t> ssid_;
  scoped_refptr<MockWiFiService> open_wifi_service_;
  scoped_refptr<MockWiFiService> wep_wifi_service_;
  scoped_refptr<MockWiFiService> eap_wifi_service_;
  MockEapCredentials *eap_;  // Owned by |eap_wifi_service_|.
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
  metrics_.RegisterService(open_wifi_service_);
  metrics_.NotifyServiceStateChanged(open_wifi_service_,
                                     Service::kStateAssociating);
  metrics_.NotifyServiceStateChanged(open_wifi_service_,
                                     Service::kStateConfiguring);
}

TEST_F(MetricsTest, WiFiServicePostReady) {
  base::TimeDelta non_zero_time_delta = base::TimeDelta::FromMilliseconds(1);
  chromeos_metrics::TimerMock *mock_time_resume_to_ready_timer =
      new chromeos_metrics::TimerMock;
  metrics_.set_time_resume_to_ready_timer(mock_time_resume_to_ready_timer);

  const int kStrength = -42;
  ExpectCommonPostReady(Metrics::kWiFiChannel2412,
                        Metrics::kWiFiNetworkPhyMode11a,
                        Metrics::kWiFiSecurityWep,
                        -kStrength);
  EXPECT_CALL(library_, SendToUMA("Network.Shill.Wifi.TimeResumeToReady",
                                  _, _, _, _)).Times(0);
  EXPECT_CALL(library_, SendEnumToUMA("Network.Shill.Wifi.EapOuterProtocol",
                                       _, _)).Times(0);
  EXPECT_CALL(library_, SendEnumToUMA("Network.Shill.Wifi.EapInnerProtocol",
                                      _, _)).Times(0);
  wep_wifi_service_->frequency_ = 2412;
  wep_wifi_service_->physical_mode_ = Metrics::kWiFiNetworkPhyMode11a;
  wep_wifi_service_->raw_signal_strength_ = kStrength;
  metrics_.RegisterService(wep_wifi_service_);
  metrics_.NotifyServiceStateChanged(wep_wifi_service_,
                                     Service::kStateConnected);
  Mock::VerifyAndClearExpectations(&library_);

  // Simulate a system suspend, resume and an AP reconnect.
  ExpectCommonPostReady(Metrics::kWiFiChannel2412,
                        Metrics::kWiFiNetworkPhyMode11a,
                        Metrics::kWiFiSecurityWep,
                        -kStrength);
  EXPECT_CALL(library_, SendToUMA("Network.Shill.Wifi.TimeResumeToReady",
                                  Ge(0),
                                  Metrics::kTimerHistogramMillisecondsMin,
                                  Metrics::kTimerHistogramMillisecondsMax,
                                  Metrics::kTimerHistogramNumBuckets));
  EXPECT_CALL(*mock_time_resume_to_ready_timer, GetElapsedTime(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(non_zero_time_delta), Return(true)));
  metrics_.NotifyPowerStateChange(PowerManagerProxyDelegate::kMem);
  metrics_.NotifyPowerStateChange(PowerManagerProxyDelegate::kOn);
  metrics_.NotifyServiceStateChanged(wep_wifi_service_,
                                     Service::kStateConnected);
  Mock::VerifyAndClearExpectations(&library_);
  Mock::VerifyAndClearExpectations(mock_time_resume_to_ready_timer);

  // Make sure subsequent connects do not count towards TimeResumeToReady.
  ExpectCommonPostReady(Metrics::kWiFiChannel2412,
                        Metrics::kWiFiNetworkPhyMode11a,
                        Metrics::kWiFiSecurityWep,
                        -kStrength);
  EXPECT_CALL(library_, SendToUMA("Network.Shill.Wifi.TimeResumeToReady",
                                  _, _, _, _)).Times(0);
  metrics_.NotifyServiceStateChanged(wep_wifi_service_,
                                     Service::kStateConnected);
}

TEST_F(MetricsTest, WiFiServicePostReadyEAP) {
  const int kStrength = -42;
  ExpectCommonPostReady(Metrics::kWiFiChannel2412,
                        Metrics::kWiFiNetworkPhyMode11a,
                        Metrics::kWiFiSecurity8021x,
                        -kStrength);
  eap_wifi_service_->frequency_ = 2412;
  eap_wifi_service_->physical_mode_ = Metrics::kWiFiNetworkPhyMode11a;
  eap_wifi_service_->raw_signal_strength_ = kStrength;
  EXPECT_CALL(*eap_, OutputConnectionMetrics(&metrics_, Technology::kWifi));
  metrics_.RegisterService(eap_wifi_service_);
  metrics_.NotifyServiceStateChanged(eap_wifi_service_,
                                     Service::kStateConnected);
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
  metrics_.NotifyDefaultServiceChanged(open_wifi_service_);

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

TEST_F(MetricsTest, TimeToConnect) {
  EXPECT_CALL(library_,
      SendToUMA("Network.Shill.Cellular.TimeToConnect",
                Ge(0),
                Metrics::kMetricTimeToConnectMillisecondsMin,
                Metrics::kMetricTimeToConnectMillisecondsMax,
                Metrics::kMetricTimeToConnectMillisecondsNumBuckets));
  const int kInterfaceIndex = 1;
  metrics_.RegisterDevice(kInterfaceIndex, Technology::kCellular);
  metrics_.NotifyDeviceConnectStarted(kInterfaceIndex, false);
  metrics_.NotifyDeviceConnectFinished(kInterfaceIndex);
}

TEST_F(MetricsTest, TimeToDisable) {
  EXPECT_CALL(library_,
      SendToUMA("Network.Shill.Cellular.TimeToDisable",
                Ge(0),
                Metrics::kMetricTimeToDisableMillisecondsMin,
                Metrics::kMetricTimeToDisableMillisecondsMax,
                Metrics::kMetricTimeToDisableMillisecondsNumBuckets));
  const int kInterfaceIndex = 1;
  metrics_.RegisterDevice(kInterfaceIndex, Technology::kCellular);
  metrics_.NotifyDeviceDisableStarted(kInterfaceIndex);
  metrics_.NotifyDeviceDisableFinished(kInterfaceIndex);
}

TEST_F(MetricsTest, TimeToEnable) {
  EXPECT_CALL(library_,
      SendToUMA("Network.Shill.Cellular.TimeToEnable",
                Ge(0),
                Metrics::kMetricTimeToEnableMillisecondsMin,
                Metrics::kMetricTimeToEnableMillisecondsMax,
                Metrics::kMetricTimeToEnableMillisecondsNumBuckets));
  const int kInterfaceIndex = 1;
  metrics_.RegisterDevice(kInterfaceIndex, Technology::kCellular);
  metrics_.NotifyDeviceEnableStarted(kInterfaceIndex);
  metrics_.NotifyDeviceEnableFinished(kInterfaceIndex);
}

TEST_F(MetricsTest, TimeToInitialize) {
  EXPECT_CALL(library_,
      SendToUMA("Network.Shill.Cellular.TimeToInitialize",
                Ge(0),
                Metrics::kMetricTimeToInitializeMillisecondsMin,
                Metrics::kMetricTimeToInitializeMillisecondsMax,
                Metrics::kMetricTimeToInitializeMillisecondsNumBuckets));
  const int kInterfaceIndex = 1;
  metrics_.RegisterDevice(kInterfaceIndex, Technology::kCellular);
  metrics_.NotifyDeviceInitialized(kInterfaceIndex);
}

TEST_F(MetricsTest, TimeToScan) {
  EXPECT_CALL(library_,
      SendToUMA("Network.Shill.Cellular.TimeToScan",
                Ge(0),
                Metrics::kMetricTimeToScanMillisecondsMin,
                Metrics::kMetricTimeToScanMillisecondsMax,
                Metrics::kMetricTimeToScanMillisecondsNumBuckets));
  const int kInterfaceIndex = 1;
  metrics_.RegisterDevice(kInterfaceIndex, Technology::kCellular);
  metrics_.NotifyDeviceScanStarted(kInterfaceIndex);
  metrics_.NotifyDeviceScanFinished(kInterfaceIndex);
}

TEST_F(MetricsTest, TimeToScanNoStart) {
  EXPECT_CALL(library_,
      SendToUMA("Network.Shill.Cellular.TimeToScan", _, _, _, _)).Times(0);
  const int kInterfaceIndex = 1;
  metrics_.RegisterDevice(kInterfaceIndex, Technology::kCellular);
  metrics_.NotifyDeviceScanFinished(kInterfaceIndex);
}

TEST_F(MetricsTest, TimeToScanIgnore) {
  // Make sure TimeToScan is not sent if the elapsed time exceeds the max
  // value.  This simulates the case where the device is in an area with no
  // service.
  const int kInterfaceIndex = 1;
  metrics_.RegisterDevice(kInterfaceIndex, Technology::kCellular);
  base::TimeDelta large_time_delta =
      base::TimeDelta::FromMilliseconds(
          Metrics::kMetricTimeToScanMillisecondsMax + 1);
  chromeos_metrics::TimerReporterMock *mock_time_to_scan_timer =
      new chromeos_metrics::TimerReporterMock;
  metrics_.set_time_to_scan_timer(kInterfaceIndex, mock_time_to_scan_timer);
  EXPECT_CALL(*mock_time_to_scan_timer, Stop()).WillOnce(Return(true));
  EXPECT_CALL(*mock_time_to_scan_timer, GetElapsedTime(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(large_time_delta), Return(true)));
  EXPECT_CALL(library_, SendToUMA(_, _, _, _, _)).Times(0);
  metrics_.NotifyDeviceScanStarted(kInterfaceIndex);
  metrics_.NotifyDeviceScanFinished(kInterfaceIndex);
}

TEST_F(MetricsTest, CellularAutoConnect) {
  EXPECT_CALL(library_,
      SendToUMA("Network.Shill.Cellular.TimeToConnect",
                Ge(0),
                Metrics::kMetricTimeToConnectMillisecondsMin,
                Metrics::kMetricTimeToConnectMillisecondsMax,
                Metrics::kMetricTimeToConnectMillisecondsNumBuckets));
  EXPECT_CALL(library_,
      SendToUMA(Metrics::kMetricCellularAutoConnectTotalTime,
                Ge(0),
                Metrics::kMetricCellularAutoConnectTotalTimeMin,
                Metrics::kMetricCellularAutoConnectTotalTimeMax,
                Metrics::kMetricCellularAutoConnectTotalTimeNumBuckets));
  EXPECT_CALL(library_,
      SendToUMA(Metrics::kMetricCellularAutoConnectTries,
                2,
                Metrics::kMetricCellularAutoConnectTriesMin,
                Metrics::kMetricCellularAutoConnectTriesMax,
                Metrics::kMetricCellularAutoConnectTriesNumBuckets));
  const int kInterfaceIndex = 1;
  metrics_.RegisterDevice(kInterfaceIndex, Technology::kCellular);
  metrics_.NotifyDeviceConnectStarted(kInterfaceIndex, true);
  metrics_.NotifyDeviceConnectStarted(kInterfaceIndex, true);
  metrics_.NotifyDeviceConnectFinished(kInterfaceIndex);
}

TEST_F(MetricsTest, CellularDrop) {
  const char *kUMATechnologyStrings[] = {
      flimflam::kNetworkTechnology1Xrtt,
      flimflam::kNetworkTechnologyEdge,
      flimflam::kNetworkTechnologyEvdo,
      flimflam::kNetworkTechnologyGprs,
      flimflam::kNetworkTechnologyGsm,
      flimflam::kNetworkTechnologyHspa,
      flimflam::kNetworkTechnologyHspaPlus,
      flimflam::kNetworkTechnologyLte,
      flimflam::kNetworkTechnologyUmts,
      "Unknown" };

  const uint16 signal_strength = 100;
  const int kInterfaceIndex = 1;
  metrics_.RegisterDevice(kInterfaceIndex, Technology::kCellular);
  for (size_t index = 0; index < arraysize(kUMATechnologyStrings); ++index) {
    EXPECT_CALL(library_,
        SendEnumToUMA(Metrics::kMetricCellularDrop,
                      index,
                      Metrics::kCellularDropTechnologyMax));
    EXPECT_CALL(library_,
        SendToUMA(Metrics::kMetricCellularSignalStrengthBeforeDrop,
                  signal_strength,
                  Metrics::kMetricCellularSignalStrengthBeforeDropMin,
                  Metrics::kMetricCellularSignalStrengthBeforeDropMax,
                  Metrics::kMetricCellularSignalStrengthBeforeDropNumBuckets));
    metrics_.NotifyCellularDeviceDrop(kInterfaceIndex,
                                      kUMATechnologyStrings[index],
                                      signal_strength);
    Mock::VerifyAndClearExpectations(&library_);
  }
}

TEST_F(MetricsTest, CellularDropsPerHour) {
  const int kInterfaceIndex = 1;
  const int kSignalStrength = 33;
  const int kNumDrops = 3;
  metrics_.RegisterDevice(kInterfaceIndex, Technology::kCellular);
  EXPECT_CALL(library_,
      SendEnumToUMA(Metrics::kMetricCellularDrop,
                    Metrics::kCellularDropTechnologyLte,
                    Metrics::kCellularDropTechnologyMax))
      .Times(kNumDrops);
  EXPECT_CALL(library_,
      SendToUMA(Metrics::kMetricCellularSignalStrengthBeforeDrop,
                kSignalStrength,
                Metrics::kMetricCellularSignalStrengthBeforeDropMin,
                Metrics::kMetricCellularSignalStrengthBeforeDropMax,
                Metrics::kMetricCellularSignalStrengthBeforeDropNumBuckets))
      .Times(kNumDrops);
  EXPECT_CALL(library_,
      SendToUMA(Metrics::kMetricCellularDropsPerHour,
                kNumDrops,
                Metrics::kMetricCellularDropsPerHourMin,
                Metrics::kMetricCellularDropsPerHourMax,
                Metrics::kMetricCellularDropsPerHourNumBuckets));
  for (int count = 0; count < kNumDrops; ++count)
    metrics_.NotifyCellularDeviceDrop(kInterfaceIndex,
                                      flimflam::kNetworkTechnologyLte,
                                      kSignalStrength);
  metrics_.HourlyTimeoutHandler();

  // Make sure the number of drops gets resetted after each hour.
  EXPECT_CALL(library_,
      SendToUMA(Metrics::kMetricCellularDropsPerHour, _, _, _, _))
      .Times(0);
  metrics_.HourlyTimeoutHandler();
}

TEST_F(MetricsTest, CellularDeviceFailure) {
  const char kErrorMessage[] =
      "org.chromium.flimflam.Error.Failure:"
      "$NWQMISTATUS: QMI_RESULT_FAILURE:QMI_ERR_CALL_FAILED#015#012#011"
      "QMI State: DISCONNECTED#015#012#011Call End Reason:1016#015#012#011"
      "Call Duration: 0 seconds#015#015#012"
      "$NWQMISTATUS: QMI_RESULT_SUCCESS:QMI_ERR_NONE#015#012#011"
      "QMI State: DISCONNECTED#015#012#011Call End Reason:0#015#012#011"
      "Call Duration: 0 seconds";
  string expected_message =
      string(Metrics::kMetricCellularFailureReason) + kErrorMessage;
  EXPECT_CALL(library_, SendUserActionToUMA(expected_message));
  Error error(Error::kOperationFailed, kErrorMessage);
  metrics_.NotifyCellularDeviceFailure(error);
}

TEST_F(MetricsTest, CellularOutOfCreditsReason) {
  EXPECT_CALL(library_,
      SendEnumToUMA(Metrics::kMetricCellularOutOfCreditsReason,
                    Metrics::kCellularOutOfCreditsReasonConnectDisconnectLoop,
                    Metrics::kCellularOutOfCreditsReasonMax));
  metrics_.NotifyCellularOutOfCredits(
      Metrics::kCellularOutOfCreditsReasonConnectDisconnectLoop);
  Mock::VerifyAndClearExpectations(&library_);

  EXPECT_CALL(library_,
      SendEnumToUMA(Metrics::kMetricCellularOutOfCreditsReason,
                    Metrics::kCellularOutOfCreditsReasonTxCongested,
                    Metrics::kCellularOutOfCreditsReasonMax));
  metrics_.NotifyCellularOutOfCredits(
      Metrics::kCellularOutOfCreditsReasonTxCongested);
  Mock::VerifyAndClearExpectations(&library_);

  EXPECT_CALL(library_,
      SendEnumToUMA(Metrics::kMetricCellularOutOfCreditsReason,
                    Metrics::kCellularOutOfCreditsReasonElongatedTimeWait,
                    Metrics::kCellularOutOfCreditsReasonMax));
  metrics_.NotifyCellularOutOfCredits(
      Metrics::kCellularOutOfCreditsReasonElongatedTimeWait);
}

TEST_F(MetricsTest, CorruptedProfile) {
  EXPECT_CALL(library_, SendEnumToUMA(Metrics::kMetricCorruptedProfile,
                                      Metrics::kCorruptedProfile,
                                      Metrics::kCorruptedProfileMax));
  metrics_.NotifyCorruptedProfile();
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
