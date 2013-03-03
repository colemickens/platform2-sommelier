// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/metrics.h"

#include <base/bind.h>
#include <base/string_util.h>
#include <base/stringprintf.h>
#include <chromeos/dbus/service_constants.h>
#include <metrics/bootstat.h>

#include "shill/ieee80211.h"
#include "shill/link_monitor.h"
#include "shill/logging.h"
#include "shill/wifi_service.h"

using base::Bind;
using base::Unretained;
using std::string;
using std::tr1::shared_ptr;

namespace shill {

// static
// Our disconnect enumeration values are 0 (System Disconnect) and
// 1 (User Disconnect), see histograms.xml, but Chrome needs a minimum
// enum value of 1 and the minimum number of buckets needs to be 3 (see
// histogram.h).  Instead of remapping System Disconnect to 1 and
// User Disconnect to 2, we can just leave the enumerated values as-is
// because Chrome implicitly creates a [0-1) bucket for us.  Using Min=1,
// Max=2 and NumBuckets=3 gives us the following three buckets:
// [0-1), [1-2), [2-INT_MAX).  We end up with an extra bucket [2-INT_MAX)
// that we can safely ignore.
const char Metrics::kMetricDisconnect[] = "Network.Shill.%s.Disconnect";
const int Metrics::kMetricDisconnectMax = 2;
const int Metrics::kMetricDisconnectMin = 1;
const int Metrics::kMetricDisconnectNumBuckets = 3;

const char Metrics::kMetricNetworkChannel[] = "Network.Shill.%s.Channel";
const int Metrics::kMetricNetworkChannelMax = Metrics::kWiFiChannelMax;
const char Metrics::kMetricNetworkEapInnerProtocol[] =
    "Network.Shill.%s.EapInnerProtocol";
const int Metrics::kMetricNetworkEapInnerProtocolMax =
    Metrics::kEapInnerProtocolMax;
const char Metrics::kMetricNetworkEapOuterProtocol[] =
    "Network.Shill.%s.EapOuterProtocol";
const int Metrics::kMetricNetworkEapOuterProtocolMax =
    Metrics::kEapOuterProtocolMax;
const char Metrics::kMetricNetworkPhyMode[] = "Network.Shill.%s.PhyMode";
const int Metrics::kMetricNetworkPhyModeMax = Metrics::kWiFiNetworkPhyModeMax;
const char Metrics::kMetricNetworkSecurity[] = "Network.Shill.%s.Security";
const int Metrics::kMetricNetworkSecurityMax = Metrics::kWiFiSecurityMax;
const char Metrics::kMetricNetworkServiceErrors[] =
    "Network.Shill.ServiceErrors";
const int Metrics::kMetricNetworkServiceErrorsMax = Service::kFailureMax;
const char Metrics::kMetricNetworkSignalStrength[] =
     "Network.Shill.%s.SignalStrength";
const int Metrics::kMetricNetworkSignalStrengthMax = 200;
const int Metrics::kMetricNetworkSignalStrengthMin = 0;
const int Metrics::kMetricNetworkSignalStrengthNumBuckets = 40;

const char Metrics::kMetricTimeOnlineSeconds[] = "Network.Shill.%s.TimeOnline";
const int Metrics::kMetricTimeOnlineSecondsMax = 8 * 60 * 60;  // 8 hours
const int Metrics::kMetricTimeOnlineSecondsMin = 1;

const char Metrics::kMetricTimeToConnectMilliseconds[] =
    "Network.Shill.%s.TimeToConnect";
const int Metrics::kMetricTimeToConnectMillisecondsMax =
    60 * 1000;  // 60 seconds
const int Metrics::kMetricTimeToConnectMillisecondsMin = 1;
const int Metrics::kMetricTimeToConnectMillisecondsNumBuckets = 60;

const char Metrics::kMetricTimeToDropSeconds[] = "Network.Shill.TimeToDrop";;
const int Metrics::kMetricTimeToDropSecondsMax = 8 * 60 * 60;  // 8 hours
const int Metrics::kMetricTimeToDropSecondsMin = 1;

const char Metrics::kMetricTimeToDisableMilliseconds[] =
    "Network.Shill.%s.TimeToDisable";
const int Metrics::kMetricTimeToDisableMillisecondsMax =
    60 * 1000;  // 60 seconds
const int Metrics::kMetricTimeToDisableMillisecondsMin = 1;
const int Metrics::kMetricTimeToDisableMillisecondsNumBuckets = 60;

const char Metrics::kMetricTimeToEnableMilliseconds[] =
    "Network.Shill.%s.TimeToEnable";
const int Metrics::kMetricTimeToEnableMillisecondsMax =
    60 * 1000;  // 60 seconds
const int Metrics::kMetricTimeToEnableMillisecondsMin = 1;
const int Metrics::kMetricTimeToEnableMillisecondsNumBuckets = 60;

const char Metrics::kMetricTimeToInitializeMilliseconds[] =
    "Network.Shill.%s.TimeToInitialize";
const int Metrics::kMetricTimeToInitializeMillisecondsMax =
    30 * 1000;  // 30 seconds
const int Metrics::kMetricTimeToInitializeMillisecondsMin = 1;
const int Metrics::kMetricTimeToInitializeMillisecondsNumBuckets = 30;

const char Metrics::kMetricTimeResumeToReadyMilliseconds[] =
    "Network.Shill.%s.TimeResumeToReady";
const char Metrics::kMetricTimeToConfigMilliseconds[] =
    "Network.Shill.%s.TimeToConfig";
const char Metrics::kMetricTimeToJoinMilliseconds[] =
    "Network.Shill.%s.TimeToJoin";
const char Metrics::kMetricTimeToOnlineMilliseconds[] =
    "Network.Shill.%s.TimeToOnline";
const char Metrics::kMetricTimeToPortalMilliseconds[] =
    "Network.Shill.%s.TimeToPortal";

const char Metrics::kMetricTimeToScanMilliseconds[] =
    "Network.Shill.%s.TimeToScan";
const int Metrics::kMetricTimeToScanMillisecondsMax = 180 * 1000;  // 3 minutes
const int Metrics::kMetricTimeToScanMillisecondsMin = 1;
const int Metrics::kMetricTimeToScanMillisecondsNumBuckets = 90;

const int Metrics::kTimerHistogramMillisecondsMax = 45 * 1000;
const int Metrics::kTimerHistogramMillisecondsMin = 1;
const int Metrics::kTimerHistogramNumBuckets = 50;

const char Metrics::kMetricPortalAttempts[] =
    "Network.Shill.%s.PortalAttempts";
const int Metrics::kMetricPortalAttemptsMax =
    PortalDetector::kMaxRequestAttempts;
const int Metrics::kMetricPortalAttemptsMin = 1;
const int Metrics::kMetricPortalAttemptsNumBuckets =
    Metrics::kMetricPortalAttemptsMax;

const char Metrics::kMetricPortalAttemptsToOnline[] =
    "Network.Shill.%s.PortalAttemptsToOnline";
const int Metrics::kMetricPortalAttemptsToOnlineMax = 100;
const int Metrics::kMetricPortalAttemptsToOnlineMin = 1;
const int Metrics::kMetricPortalAttemptsToOnlineNumBuckets = 10;

const char Metrics::kMetricPortalResult[] = "Network.Shill.%s.PortalResult";

const char Metrics::kMetricTerminationActionTimeOnTerminate[] =
    "Network.Shill.TerminationActionTime.OnTerminate";
const char Metrics::kMetricTerminationActionResultOnTerminate[] =
    "Network.Shill.TerminationActionResult.OnTerminate";
const char Metrics::kMetricTerminationActionTimeOnSuspend[] =
    "Network.Shill.TerminationActionTime.OnSuspend";
const char Metrics::kMetricTerminationActionResultOnSuspend[] =
    "Network.Shill.TerminationActionResult.OnSuspend";
const int Metrics::kMetricTerminationActionTimeMillisecondsMax = 10000;
const int Metrics::kMetricTerminationActionTimeMillisecondsMin = 1;

// static
const char Metrics::kMetricServiceFixupEntries[] =
    "Network.Shill.%s.ServiceFixupEntries";

// static
const uint16 Metrics::kWiFiBandwidth5MHz = 5;
const uint16 Metrics::kWiFiBandwidth20MHz = 20;
const uint16 Metrics::kWiFiFrequency2412 = 2412;
const uint16 Metrics::kWiFiFrequency2472 = 2472;
const uint16 Metrics::kWiFiFrequency2484 = 2484;
const uint16 Metrics::kWiFiFrequency5170 = 5170;
const uint16 Metrics::kWiFiFrequency5180 = 5180;
const uint16 Metrics::kWiFiFrequency5230 = 5230;
const uint16 Metrics::kWiFiFrequency5240 = 5240;
const uint16 Metrics::kWiFiFrequency5320 = 5320;
const uint16 Metrics::kWiFiFrequency5500 = 5500;
const uint16 Metrics::kWiFiFrequency5700 = 5700;
const uint16 Metrics::kWiFiFrequency5745 = 5745;
const uint16 Metrics::kWiFiFrequency5825 = 5825;

// static
const char Metrics::kMetricPowerManagerKey[] = "metrics";

// static
const char Metrics::kMetricLinkMonitorFailure[] =
    "Network.Shill.%s.LinkMonitorFailure";
const char Metrics::kMetricLinkMonitorResponseTimeSample[] =
    "Network.Shill.%s.LinkMonitorResponseTimeSample";
const int Metrics::kMetricLinkMonitorResponseTimeSampleMin = 0;
const int Metrics::kMetricLinkMonitorResponseTimeSampleMax =
    LinkMonitor::kTestPeriodMilliseconds;
const int Metrics::kMetricLinkMonitorResponseTimeSampleNumBuckets = 50;
const char Metrics::kMetricLinkMonitorSecondsToFailure[] =
    "Network.Shill.%s.LinkMonitorSecondsToFailure";
const int Metrics::kMetricLinkMonitorSecondsToFailureMin = 0;
const int Metrics::kMetricLinkMonitorSecondsToFailureMax = 7200;
const int Metrics::kMetricLinkMonitorSecondsToFailureNumBuckets = 50;
const char Metrics::kMetricLinkMonitorBroadcastErrorsAtFailure[] =
    "Network.Shill.%s.LinkMonitorBroadcastErrorsAtFailure";
const char Metrics::kMetricLinkMonitorUnicastErrorsAtFailure[] =
    "Network.Shill.%s.LinkMonitorUnicastErrorsAtFailure";
const int Metrics::kMetricLinkMonitorErrorCountMin = 0;
const int Metrics::kMetricLinkMonitorErrorCountMax =
    LinkMonitor::kFailureThreshold;
const int Metrics::kMetricLinkMonitorErrorCountNumBuckets =
    LinkMonitor::kFailureThreshold + 1;

// static
const char Metrics::kMetricLinkClientDisconnectReason[] =
    "Network.Shill.WiFi.ClientDisconnectReason";
const char Metrics::kMetricLinkApDisconnectReason[] =
    "Network.Shill.WiFi.ApDisconnectReason";
const char Metrics::kMetricLinkClientDisconnectType[] =
    "Network.Shill.WiFi.ClientDisconnectType";
const char Metrics::kMetricLinkApDisconnectType[] =
    "Network.Shill.WiFi.ApDisconnectType";

// static
const char Metrics::kMetricCellularDrop[] =
    "Network.Shill.Cellular.Drop";
const char Metrics::kMetricCellularDropsPerHour[] =
    "Network.Shill.Cellular.DropsPerHour";
const int Metrics::kMetricCellularDropsPerHourMax = 60;
const int Metrics::kMetricCellularDropsPerHourMin = 1;
const int Metrics::kMetricCellularDropsPerHourNumBuckets = 10;
// The format of FailureReason is different to other metrics because this
// name is prepended to the error message before the entire string is sent
// via SendUserActionToUMA.
const char Metrics::kMetricCellularFailureReason[] =
    "Network.Shill.Cellular.FailureReason: ";
const char Metrics::kMetricCellularSignalStrengthBeforeDrop[] =
    "Network.Shill.Cellular.SignalStrengthBeforeDrop";
const int Metrics::kMetricCellularSignalStrengthBeforeDropMax = 100;
const int Metrics::kMetricCellularSignalStrengthBeforeDropMin = 0;
const int Metrics::kMetricCellularSignalStrengthBeforeDropNumBuckets = 10;
const char Metrics::kMetricCellularAutoConnectTries[] =
    "Network.Shill.Cellular.AutoConnectTries";
const int Metrics::kMetricCellularAutoConnectTriesMax = 20;
const int Metrics::kMetricCellularAutoConnectTriesMin = 1;
const int Metrics::kMetricCellularAutoConnectTriesNumBuckets = 20;
const char Metrics::kMetricCellularAutoConnectTotalTime[] =
    "Network.Shill.Cellular.AutoConnectTotalTime";
const int Metrics::kMetricCellularAutoConnectTotalTimeMax =
    60 * 1000;  // 60 seconds
const int Metrics::kMetricCellularAutoConnectTotalTimeMin = 0;
const int Metrics::kMetricCellularAutoConnectTotalTimeNumBuckets = 60;

// static
const char Metrics::kMetricCorruptedProfile[] =
    "Network.Shill.CorruptedProfile";

// static
const char Metrics::kMetricVpnDriver[] =
    "Network.Shill.Vpn.Driver";
const int Metrics::kMetricVpnDriverMax = Metrics::kVpnDriverMax;
const char Metrics::kMetricVpnRemoteAuthenticationType[] =
    "Network.Shill.Vpn.RemoteAuthenticationType";
const int Metrics::kMetricVpnRemoteAuthenticationTypeMax =
    Metrics::kVpnRemoteAuthenticationTypeMax;
const char Metrics::kMetricVpnUserAuthenticationType[] =
    "Network.Shill.Vpn.UserAuthenticationType";
const int Metrics::kMetricVpnUserAuthenticationTypeMax =
    Metrics::kVpnUserAuthenticationTypeMax;

// static
const int Metrics::kHourlyTimeoutMilliseconds = 3600 * 1000;  // One hour


Metrics::Metrics(EventDispatcher *dispatcher)
    : dispatcher_(dispatcher),
      library_(&metrics_library_),
      last_default_technology_(Technology::kUnknown),
      was_online_(false),
      time_online_timer_(new chromeos_metrics::Timer),
      time_to_drop_timer_(new chromeos_metrics::Timer),
      time_resume_to_ready_timer_(new chromeos_metrics::Timer),
      time_termination_actions_timer(new chromeos_metrics::Timer),
      collect_bootstats_(true) {
  metrics_library_.Init();
  chromeos_metrics::TimerReporter::set_metrics_lib(library_);
}

Metrics::~Metrics() {}

// static
Metrics::WiFiChannel Metrics::WiFiFrequencyToChannel(uint16 frequency) {
  WiFiChannel channel = kWiFiChannelUndef;
  if (kWiFiFrequency2412 <= frequency && frequency <= kWiFiFrequency2472) {
    if (((frequency - kWiFiFrequency2412) % kWiFiBandwidth5MHz) == 0)
      channel = static_cast<WiFiChannel>(
                    kWiFiChannel2412 +
                    (frequency - kWiFiFrequency2412) / kWiFiBandwidth5MHz);
  } else if (frequency == kWiFiFrequency2484) {
    channel = kWiFiChannel2484;
  } else if (kWiFiFrequency5170 <= frequency &&
             frequency <= kWiFiFrequency5230) {
    if ((frequency % kWiFiBandwidth20MHz) == 0)
      channel = static_cast<WiFiChannel>(
                    kWiFiChannel5180 +
                    (frequency - kWiFiFrequency5180) / kWiFiBandwidth20MHz);
    if ((frequency % kWiFiBandwidth20MHz) == 10)
      channel = static_cast<WiFiChannel>(
                    kWiFiChannel5170 +
                    (frequency - kWiFiFrequency5170) / kWiFiBandwidth20MHz);
  } else if (kWiFiFrequency5240 <= frequency &&
             frequency <= kWiFiFrequency5320) {
    if (((frequency - kWiFiFrequency5180) % kWiFiBandwidth20MHz) == 0)
      channel = static_cast<WiFiChannel>(
                    kWiFiChannel5180 +
                    (frequency - kWiFiFrequency5180) / kWiFiBandwidth20MHz);
  } else if (kWiFiFrequency5500 <= frequency &&
             frequency <= kWiFiFrequency5700) {
    if (((frequency - kWiFiFrequency5500) % kWiFiBandwidth20MHz) == 0)
      channel = static_cast<WiFiChannel>(
                    kWiFiChannel5500 +
                    (frequency - kWiFiFrequency5500) / kWiFiBandwidth20MHz);
  } else if (kWiFiFrequency5745 <= frequency &&
             frequency <= kWiFiFrequency5825) {
    if (((frequency - kWiFiFrequency5745) % kWiFiBandwidth20MHz) == 0)
      channel = static_cast<WiFiChannel>(
                    kWiFiChannel5745 +
                    (frequency - kWiFiFrequency5745) / kWiFiBandwidth20MHz);
  }
  CHECK(kWiFiChannelUndef <= channel && channel < kWiFiChannelMax);

  if (channel == kWiFiChannelUndef)
    LOG(WARNING) << "no mapping for frequency " << frequency;
  else
    SLOG(Metrics, 3) << "map " << frequency << " to " << channel;

  return channel;
}

// static
Metrics::WiFiSecurity Metrics::WiFiSecurityStringToEnum(
    const std::string &security) {
  if (security == flimflam::kSecurityNone) {
    return kWiFiSecurityNone;
  } else if (security == flimflam::kSecurityWep) {
    return kWiFiSecurityWep;
  } else if (security == flimflam::kSecurityWpa) {
    return kWiFiSecurityWpa;
  } else if (security == flimflam::kSecurityRsn) {
    return kWiFiSecurityRsn;
  } else if (security == flimflam::kSecurity8021x) {
    return kWiFiSecurity8021x;
  } else if (security == flimflam::kSecurityPsk) {
    return kWiFiSecurityPsk;
  } else {
    return kWiFiSecurityUnknown;
  }
}

// static
Metrics::EapOuterProtocol Metrics::EapOuterProtocolStringToEnum(
    const std::string &outer) {
  if (outer == flimflam::kEapMethodPEAP) {
    return kEapOuterProtocolPeap;
  } else if (outer == flimflam::kEapMethodTLS) {
    return kEapOuterProtocolTls;
  } else if (outer == flimflam::kEapMethodTTLS) {
    return kEapOuterProtocolTtls;
  } else if (outer == flimflam::kEapMethodLEAP) {
    return kEapOuterProtocolLeap;
  } else {
    return kEapOuterProtocolUnknown;
  }
}

// static
Metrics::EapInnerProtocol Metrics::EapInnerProtocolStringToEnum(
    const std::string &inner) {
  if (inner.empty()) {
    return kEapInnerProtocolNone;
  } else if (inner == flimflam::kEapPhase2AuthPEAPMD5) {
    return kEapInnerProtocolPeapMd5;
  } else if (inner == flimflam::kEapPhase2AuthPEAPMSCHAPV2) {
    return kEapInnerProtocolPeapMschapv2;
  } else if (inner == flimflam::kEapPhase2AuthTTLSEAPMD5) {
    return kEapInnerProtocolTtlsEapMd5;
  } else if (inner == flimflam::kEapPhase2AuthTTLSEAPMSCHAPV2) {
    return kEapInnerProtocolTtlsEapMschapv2;
  } else if (inner == flimflam::kEapPhase2AuthTTLSMSCHAPV2) {
    return kEapInnerProtocolTtlsMschapv2;
  } else if (inner == flimflam::kEapPhase2AuthTTLSMSCHAP) {
    return kEapInnerProtocolTtlsMschap;
  } else if (inner == flimflam::kEapPhase2AuthTTLSPAP) {
    return kEapInnerProtocolTtlsPap;
  } else if (inner == flimflam::kEapPhase2AuthTTLSCHAP) {
    return kEapInnerProtocolTtlsChap;
  } else {
    return kEapInnerProtocolUnknown;
  }
}

// static
Metrics::PortalResult Metrics::PortalDetectionResultToEnum(
      const PortalDetector::Result &result) {
  DCHECK(result.final);
  PortalResult retval = kPortalResultUnknown;
  // The only time we should end a successful portal detection is when we're
  // in the Content phase.  If we end with kStatusSuccess in any other phase,
  // then this indicates that something bad has happened.
  switch (result.phase) {
    case PortalDetector::kPhaseDNS:
      if (result.status == PortalDetector::kStatusFailure)
        retval = kPortalResultDNSFailure;
      else if (result.status == PortalDetector::kStatusTimeout)
        retval = kPortalResultDNSTimeout;
      else
        LOG(DFATAL) << __func__ << ": Final result status " << result.status
                    << " is not allowed in the DNS phase";
      break;

    case PortalDetector::kPhaseConnection:
      if (result.status == PortalDetector::kStatusFailure)
        retval = kPortalResultConnectionFailure;
      else if (result.status == PortalDetector::kStatusTimeout)
        retval = kPortalResultConnectionTimeout;
      else
        LOG(DFATAL) << __func__ << ": Final result status " << result.status
                    << " is not allowed in the Connection phase";
      break;

    case PortalDetector::kPhaseHTTP:
      if (result.status == PortalDetector::kStatusFailure)
        retval = kPortalResultHTTPFailure;
      else if (result.status == PortalDetector::kStatusTimeout)
        retval = kPortalResultHTTPTimeout;
      else
        LOG(DFATAL) << __func__ << ": Final result status " << result.status
                    << " is not allowed in the HTTP phase";
      break;

    case PortalDetector::kPhaseContent:
      if (result.status == PortalDetector::kStatusSuccess)
        retval = kPortalResultSuccess;
      else if (result.status == PortalDetector::kStatusFailure)
        retval = kPortalResultContentFailure;
      else if (result.status == PortalDetector::kStatusTimeout)
        retval = kPortalResultContentTimeout;
      else
        LOG(DFATAL) << __func__ << ": Final result status " << result.status
                    << " is not allowed in the Content phase";
      break;

    case PortalDetector::kPhaseUnknown:
      retval = kPortalResultUnknown;
      break;

    default:
      LOG(DFATAL) << __func__ << ": Invalid phase " << result.phase;
      break;
  }

  return retval;
}

void Metrics::Start() {
  SLOG(Metrics, 2) << __func__;
  hourly_timeout_handler_.Reset(
      Bind(&Metrics::HourlyTimeoutHandler, Unretained(this)));
  dispatcher_->PostDelayedTask(hourly_timeout_handler_.callback(),
                               kHourlyTimeoutMilliseconds);
}

void Metrics::Stop() {
  SLOG(Metrics, 2) << __func__;
  hourly_timeout_handler_.Cancel();
}

void Metrics::RegisterService(const Service *service) {
  shared_ptr<ServiceMetrics> service_metrics(new ServiceMetrics);
  services_metrics_[service] = service_metrics;
  service_metrics->service = service;
  InitializeCommonServiceMetrics(service);
  service->InitializeCustomMetrics();
}

void Metrics::DeregisterService(const Service *service) {
  services_metrics_.erase(service);
}

void Metrics::AddServiceStateTransitionTimer(
    const Service *service,
    const string &histogram_name,
    Service::ConnectState start_state,
    Service::ConnectState stop_state) {
  ServiceMetricsLookupMap::iterator it = services_metrics_.find(service);
  if (it == services_metrics_.end()) {
    SLOG(Metrics, 1) << "service not found";
    DCHECK(false);
    return;
  }
  ServiceMetrics *service_metrics = it->second.get();
  CHECK(start_state < stop_state);
  chromeos_metrics::TimerReporter *timer =
      new chromeos_metrics::TimerReporter(histogram_name,
                                          kTimerHistogramMillisecondsMin,
                                          kTimerHistogramMillisecondsMax,
                                          kTimerHistogramNumBuckets);
  service_metrics->timers.push_back(timer);  // passes ownership.
  service_metrics->start_on_state[start_state].push_back(timer);
  service_metrics->stop_on_state[stop_state].push_back(timer);
}

void Metrics::NotifyDefaultServiceChanged(const Service *service) {
  base::TimeDelta elapsed_seconds;

  Technology::Identifier technology = (service) ? service->technology() :
                                                  Technology::kUnknown;
  if (technology != last_default_technology_) {
    if (last_default_technology_ != Technology::kUnknown) {
      string histogram = GetFullMetricName(kMetricTimeOnlineSeconds,
                                           last_default_technology_);
      time_online_timer_->GetElapsedTime(&elapsed_seconds);
      SendToUMA(histogram,
                elapsed_seconds.InSeconds(),
                kMetricTimeOnlineSecondsMin,
                kMetricTimeOnlineSecondsMax,
                kTimerHistogramNumBuckets);
    }
    last_default_technology_ = technology;
    time_online_timer_->Start();
  }

  // Ignore changes that are not online/offline transitions; e.g.
  // switching between wired and wireless.  TimeToDrop measures
  // time online regardless of how we are connected.
  if ((service == NULL && !was_online_) || (service != NULL && was_online_))
    return;

  if (service == NULL) {
    time_to_drop_timer_->GetElapsedTime(&elapsed_seconds);
    SendToUMA(kMetricTimeToDropSeconds,
              elapsed_seconds.InSeconds(),
              kMetricTimeToDropSecondsMin,
              kMetricTimeToDropSecondsMax,
              kTimerHistogramNumBuckets);
  } else {
    time_to_drop_timer_->Start();
  }

  was_online_ = (service != NULL);
}

void Metrics::NotifyServiceStateChanged(const Service *service,
                                        Service::ConnectState new_state) {
  ServiceMetricsLookupMap::iterator it = services_metrics_.find(service);
  if (it == services_metrics_.end()) {
    SLOG(Metrics, 1) << "service not found";
    DCHECK(false);
    return;
  }
  ServiceMetrics *service_metrics = it->second.get();
  UpdateServiceStateTransitionMetrics(service_metrics, new_state);

  if (new_state == Service::kStateFailure)
    SendServiceFailure(service);

  if (collect_bootstats_) {
    bootstat_log(
        StringPrintf("network-%s-%s",
                     Technology::NameFromIdentifier(
                         service->technology()).c_str(),
                     service->GetStateString().c_str()).c_str());
  }

  if (new_state != Service::kStateConnected)
    return;

  base::TimeDelta time_resume_to_ready;
  time_resume_to_ready_timer_->GetElapsedTime(&time_resume_to_ready);
  time_resume_to_ready_timer_->Reset();
  service->SendPostReadyStateMetrics(time_resume_to_ready.InMilliseconds());
}

string Metrics::GetFullMetricName(const char *metric_name,
                                  Technology::Identifier technology_id) {
  string technology = Technology::NameFromIdentifier(technology_id);
  technology[0] = base::ToUpperASCII(technology[0]);
  return base::StringPrintf(metric_name, technology.c_str());
}

void Metrics::NotifyServiceDisconnect(const Service *service) {
  Technology::Identifier technology = service->technology();
  string histogram = GetFullMetricName(kMetricDisconnect, technology);
  SendToUMA(histogram,
            service->explicitly_disconnected(),
            kMetricDisconnectMin,
            kMetricDisconnectMax,
            kMetricDisconnectNumBuckets);
}

void Metrics::NotifyPowerStateChange(PowerManager::SuspendState new_state) {
  if (new_state == PowerManagerProxyDelegate::kOn) {
    time_resume_to_ready_timer_->Start();
  } else {
    time_resume_to_ready_timer_->Reset();
  }
}

void Metrics::NotifyTerminationActionsStarted(
    TerminationActionReason /*reason*/) {
  if (time_termination_actions_timer->HasStarted())
    return;
  time_termination_actions_timer->Start();
}

void Metrics::NotifyTerminationActionsCompleted(
    TerminationActionReason reason, bool success) {
  if (!time_termination_actions_timer->HasStarted())
    return;

  int result = success ? kTerminationActionResultSuccess :
                         kTerminationActionResultFailure;

  base::TimeDelta elapsed_time;
  time_termination_actions_timer->GetElapsedTime(&elapsed_time);
  time_termination_actions_timer->Reset();
  string time_metric, result_metric;
  switch (reason) {
  case kTerminationActionReasonSuspend:
    time_metric = kMetricTerminationActionTimeOnSuspend;
    result_metric = kMetricTerminationActionResultOnSuspend;
    break;
  case kTerminationActionReasonTerminate:
    time_metric = kMetricTerminationActionTimeOnTerminate;
    result_metric = kMetricTerminationActionResultOnTerminate;
    break;
  }

  SendToUMA(time_metric,
            elapsed_time.InMilliseconds(),
            kMetricTerminationActionTimeMillisecondsMin,
            kMetricTerminationActionTimeMillisecondsMax,
            kTimerHistogramNumBuckets);

  SendEnumToUMA(result_metric,
                result,
                kTerminationActionResultMax);
}

void Metrics::NotifyLinkMonitorFailure(
    Technology::Identifier technology,
    LinkMonitorFailure failure,
    int seconds_to_failure,
    int broadcast_error_count,
    int unicast_error_count) {
  string histogram = GetFullMetricName(kMetricLinkMonitorFailure,
                                       technology);
  SendEnumToUMA(histogram, failure, kLinkMonitorFailureMax);

  if (failure == kLinkMonitorFailureThresholdReached) {
    if (seconds_to_failure > kMetricLinkMonitorSecondsToFailureMax) {
      seconds_to_failure = kMetricLinkMonitorSecondsToFailureMax;
    }
    histogram = GetFullMetricName(kMetricLinkMonitorSecondsToFailure,
                                  technology);
    SendToUMA(histogram,
              seconds_to_failure,
              kMetricLinkMonitorSecondsToFailureMin,
              kMetricLinkMonitorSecondsToFailureMax,
              kMetricLinkMonitorSecondsToFailureNumBuckets);
    histogram = GetFullMetricName(kMetricLinkMonitorBroadcastErrorsAtFailure,
                                  technology);
    SendToUMA(histogram,
              broadcast_error_count,
              kMetricLinkMonitorErrorCountMin,
              kMetricLinkMonitorErrorCountMax,
              kMetricLinkMonitorErrorCountNumBuckets);
    histogram = GetFullMetricName(kMetricLinkMonitorUnicastErrorsAtFailure,
                                  technology);
    SendToUMA(histogram,
              unicast_error_count,
              kMetricLinkMonitorErrorCountMin,
              kMetricLinkMonitorErrorCountMax,
              kMetricLinkMonitorErrorCountNumBuckets);
  }
}

void Metrics::NotifyLinkMonitorResponseTimeSampleAdded(
    Technology::Identifier technology,
    int response_time_milliseconds) {
  string histogram = GetFullMetricName(kMetricLinkMonitorResponseTimeSample,
                                       technology);
  SendToUMA(histogram,
            response_time_milliseconds,
            kMetricLinkMonitorResponseTimeSampleMin,
            kMetricLinkMonitorResponseTimeSampleMax,
            kMetricLinkMonitorResponseTimeSampleNumBuckets);
}

void Metrics::Notify80211Disconnect(WiFiDisconnectByWhom by_whom,
                                    IEEE_80211::WiFiReasonCode reason) {
  string metric_disconnect_reason;
  string metric_disconnect_type;
  WiFiStatusType type;

  if (by_whom == kDisconnectedByAp) {
    metric_disconnect_reason = kMetricLinkApDisconnectReason;
    metric_disconnect_type = kMetricLinkApDisconnectType;
    type = kStatusCodeTypeByAp;
  } else {
    metric_disconnect_reason = kMetricLinkClientDisconnectReason;
    metric_disconnect_type = kMetricLinkClientDisconnectType;
    switch(reason) {
      case IEEE_80211::kReasonCodeSenderHasLeft:
      case IEEE_80211::kReasonCodeDisassociatedHasLeft:
        type = kStatusCodeTypeByUser;
        break;

      case IEEE_80211::kReasonCodeInactivity:
        type = kStatusCodeTypeConsideredDead;
        break;

      default:
        type = kStatusCodeTypeByClient;
        break;
    }
  }
  SendEnumToUMA(metric_disconnect_reason, reason,
                IEEE_80211::kStatusCodeMax);
  SendEnumToUMA(metric_disconnect_type, type, kStatusCodeTypeMax);
}

void Metrics::RegisterDevice(int interface_index,
                             Technology::Identifier technology) {
  SLOG(Metrics, 2) << __func__ << ": " << interface_index;
  shared_ptr<DeviceMetrics> device_metrics(new DeviceMetrics);
  devices_metrics_[interface_index] = device_metrics;
  device_metrics->technology = technology;
  string histogram = GetFullMetricName(kMetricTimeToInitializeMilliseconds,
                                       technology);
  device_metrics->initialization_timer.reset(
      new chromeos_metrics::TimerReporter(
          histogram,
          kMetricTimeToInitializeMillisecondsMin,
          kMetricTimeToInitializeMillisecondsMax,
          kMetricTimeToInitializeMillisecondsNumBuckets));
  device_metrics->initialization_timer->Start();
  histogram = GetFullMetricName(kMetricTimeToEnableMilliseconds,
                                technology);
  device_metrics->enable_timer.reset(
      new chromeos_metrics::TimerReporter(
          histogram,
          kMetricTimeToEnableMillisecondsMin,
          kMetricTimeToEnableMillisecondsMax,
          kMetricTimeToEnableMillisecondsNumBuckets));
  histogram = GetFullMetricName(kMetricTimeToDisableMilliseconds,
                                technology);
  device_metrics->disable_timer.reset(
      new chromeos_metrics::TimerReporter(
          histogram,
          kMetricTimeToDisableMillisecondsMin,
          kMetricTimeToDisableMillisecondsMax,
          kMetricTimeToDisableMillisecondsNumBuckets));
  histogram = GetFullMetricName(kMetricTimeToScanMilliseconds,
                                technology);
  device_metrics->scan_timer.reset(
      new chromeos_metrics::TimerReporter(
          histogram,
          kMetricTimeToScanMillisecondsMin,
          kMetricTimeToScanMillisecondsMax,
          kMetricTimeToScanMillisecondsNumBuckets));
  histogram = GetFullMetricName(kMetricTimeToConnectMilliseconds,
                                technology);
  device_metrics->connect_timer.reset(
      new chromeos_metrics::TimerReporter(
          histogram,
          kMetricTimeToConnectMillisecondsMin,
          kMetricTimeToConnectMillisecondsMax,
          kMetricTimeToConnectMillisecondsNumBuckets));
  device_metrics->auto_connect_timer.reset(
      new chromeos_metrics::TimerReporter(
          kMetricCellularAutoConnectTotalTime,
          kMetricCellularAutoConnectTotalTimeMin,
          kMetricCellularAutoConnectTotalTimeMax,
          kMetricCellularAutoConnectTotalTimeNumBuckets));
}

bool Metrics::IsDeviceRegistered(int interface_index,
                                 Technology::Identifier technology) {
  SLOG(Metrics, 2) << __func__ << ": interface index: " << interface_index
                               << ", technology: " << technology;
  DeviceMetrics *device_metrics = GetDeviceMetrics(interface_index);
  if (device_metrics == NULL)
    return false;
  // Make sure the device technologies match.
  return (technology == device_metrics->technology);
}

void Metrics::DeregisterDevice(int interface_index) {
  SLOG(Metrics, 2) << __func__ << ": interface index: " << interface_index;
  devices_metrics_.erase(interface_index);
}

void Metrics::NotifyDeviceInitialized(int interface_index) {
  DeviceMetrics *device_metrics = GetDeviceMetrics(interface_index);
  if (device_metrics == NULL)
    return;
  device_metrics->initialization_timer->Stop();
  device_metrics->initialization_timer->ReportMilliseconds();
}

void Metrics::NotifyDeviceEnableStarted(int interface_index) {
  DeviceMetrics *device_metrics = GetDeviceMetrics(interface_index);
  if (device_metrics == NULL)
    return;
  device_metrics->enable_timer->Start();
}

void Metrics::NotifyDeviceEnableFinished(int interface_index) {
  DeviceMetrics *device_metrics = GetDeviceMetrics(interface_index);
  if (device_metrics == NULL)
    return;
  device_metrics->enable_timer->Stop();
  device_metrics->enable_timer->ReportMilliseconds();
}

void Metrics::NotifyDeviceDisableStarted(int interface_index) {
  DeviceMetrics *device_metrics = GetDeviceMetrics(interface_index);
  if (device_metrics == NULL)
    return;
  device_metrics->disable_timer->Start();
}

void Metrics::NotifyDeviceDisableFinished(int interface_index) {
  DeviceMetrics *device_metrics = GetDeviceMetrics(interface_index);
  if (device_metrics == NULL)
    return;
  device_metrics->disable_timer->Stop();
  device_metrics->disable_timer->ReportMilliseconds();
}

void Metrics::NotifyDeviceScanStarted(int interface_index) {
  DeviceMetrics *device_metrics = GetDeviceMetrics(interface_index);
  if (device_metrics == NULL)
    return;
  // This metric is only supported for cellular devices.
  if (device_metrics->technology != Technology::kCellular)
    return;
  device_metrics->scan_timer->Start();
}

void Metrics::NotifyDeviceScanFinished(int interface_index) {
  DeviceMetrics *device_metrics = GetDeviceMetrics(interface_index);
  if (device_metrics == NULL)
    return;
  // This metric is only supported for cellular devices.
  if (device_metrics->technology != Technology::kCellular)
    return;
  device_metrics->scan_timer->Stop();
  // Don't send TimeToScan metrics if the elapsed time exceeds the max
  // metrics value.  This usually means that the modem is in an area
  // without service and we're not interested in this scenario.
  base::TimeDelta elapsed_time;
  device_metrics->scan_timer->GetElapsedTime(&elapsed_time);
  if (elapsed_time.InMilliseconds() <= kMetricTimeToScanMillisecondsMax)
    device_metrics->scan_timer->ReportMilliseconds();
}

void Metrics::NotifyDeviceConnectStarted(int interface_index,
                                         bool is_auto_connecting) {
  DeviceMetrics *device_metrics = GetDeviceMetrics(interface_index);
  if (device_metrics == NULL)
    return;
  device_metrics->connect_timer->Start();

  if (is_auto_connecting) {
    device_metrics->auto_connect_tries++;
    if (device_metrics->auto_connect_tries == 1)
      device_metrics->auto_connect_timer->Start();
  } else {
    AutoConnectMetricsReset(device_metrics);
  }
}

void Metrics::NotifyDeviceConnectFinished(int interface_index) {
  DeviceMetrics *device_metrics = GetDeviceMetrics(interface_index);
  if (device_metrics == NULL)
    return;
  device_metrics->connect_timer->Stop();
  device_metrics->connect_timer->ReportMilliseconds();

  if (device_metrics->auto_connect_tries > 0) {
    device_metrics->auto_connect_timer->Stop();
    base::TimeDelta elapsed_time;
    device_metrics->auto_connect_timer->GetElapsedTime(&elapsed_time);
    if (elapsed_time.InMilliseconds() > kMetricCellularAutoConnectTotalTimeMax)
      return;
    device_metrics->auto_connect_timer->ReportMilliseconds();
    SendToUMA(kMetricCellularAutoConnectTries,
              device_metrics->auto_connect_tries,
              kMetricCellularAutoConnectTriesMin,
              kMetricCellularAutoConnectTriesMax,
              kMetricCellularAutoConnectTriesNumBuckets);
    AutoConnectMetricsReset(device_metrics);
  }
}

void Metrics::NotifyCellularDeviceDrop(int interface_index,
                                       const string &network_technology,
                                       uint16 signal_strength) {
  SLOG(Metrics, 2) << __func__ << ": " << network_technology
                               << ", " << signal_strength;
  CellularDropTechnology drop_technology = kCellularDropTechnologyUnknown;
  if (network_technology == flimflam::kNetworkTechnology1Xrtt) {
    drop_technology = kCellularDropTechnology1Xrtt;
  } else if (network_technology == flimflam::kNetworkTechnologyEdge) {
    drop_technology = kCellularDropTechnologyEdge;
  } else if (network_technology == flimflam::kNetworkTechnologyEvdo) {
    drop_technology = kCellularDropTechnologyEvdo;
  } else if (network_technology == flimflam::kNetworkTechnologyGprs) {
    drop_technology = kCellularDropTechnologyGprs;
  } else if (network_technology == flimflam::kNetworkTechnologyGsm) {
    drop_technology = kCellularDropTechnologyGsm;
  } else if (network_technology == flimflam::kNetworkTechnologyHspa) {
    drop_technology = kCellularDropTechnologyHspa;
  } else if (network_technology == flimflam::kNetworkTechnologyHspaPlus) {
    drop_technology = kCellularDropTechnologyHspaPlus;
  } else if (network_technology == flimflam::kNetworkTechnologyLte) {
    drop_technology = kCellularDropTechnologyLte;
  } else if (network_technology == flimflam::kNetworkTechnologyUmts) {
    drop_technology = kCellularDropTechnologyUmts;
  }
  SendEnumToUMA(kMetricCellularDrop,
                drop_technology,
                kCellularDropTechnologyMax);
  SendToUMA(kMetricCellularSignalStrengthBeforeDrop,
            signal_strength,
            kMetricCellularSignalStrengthBeforeDropMin,
            kMetricCellularSignalStrengthBeforeDropMax,
            kMetricCellularSignalStrengthBeforeDropNumBuckets);

  DeviceMetrics *device_metrics = GetDeviceMetrics(interface_index);
  if (device_metrics == NULL)
    return;
  device_metrics->num_drops++;
}

void Metrics::NotifyCellularDeviceFailure(const Error &error) {
  library_->SendUserActionToUMA(
      kMetricCellularFailureReason + error.message());
}

void Metrics::NotifyCorruptedProfile() {
  SendEnumToUMA(kMetricCorruptedProfile,
                kCorruptedProfile,
                kCorruptedProfileMax);
}

bool Metrics::SendEnumToUMA(const string &name, int sample, int max) {
  return library_->SendEnumToUMA(name, sample, max);
}

bool Metrics::SendToUMA(const string &name, int sample, int min, int max,
                        int num_buckets) {
  return library_->SendToUMA(name, sample, min, max, num_buckets);
}

void Metrics::InitializeCommonServiceMetrics(const Service *service) {
  Technology::Identifier technology = service->technology();
  string histogram = GetFullMetricName(kMetricTimeToConfigMilliseconds,
                                       technology);
  AddServiceStateTransitionTimer(
      service,
      histogram,
      Service::kStateConfiguring,
      Service::kStateConnected);
  histogram = GetFullMetricName(kMetricTimeToPortalMilliseconds, technology);
  AddServiceStateTransitionTimer(
      service,
      histogram,
      Service::kStateConnected,
      Service::kStatePortal);
  histogram = GetFullMetricName(kMetricTimeToOnlineMilliseconds, technology);
  AddServiceStateTransitionTimer(
      service,
      histogram,
      Service::kStateConnected,
      Service::kStateOnline);
}

void Metrics::UpdateServiceStateTransitionMetrics(
    ServiceMetrics *service_metrics,
    Service::ConnectState new_state) {
  TimerReportersList::iterator it;
  TimerReportersList &start_timers = service_metrics->start_on_state[new_state];
  for (it = start_timers.begin(); it != start_timers.end(); ++it)
    (*it)->Start();

  TimerReportersList &stop_timers = service_metrics->stop_on_state[new_state];
  for (it = stop_timers.begin(); it != stop_timers.end(); ++it) {
    (*it)->Stop();
    (*it)->ReportMilliseconds();
  }
}

void Metrics::SendServiceFailure(const Service *service) {
  library_->SendEnumToUMA(kMetricNetworkServiceErrors,
                          service->failure(),
                          kMetricNetworkServiceErrorsMax);
}

Metrics::DeviceMetrics *Metrics::GetDeviceMetrics(int interface_index) const {
  DeviceMetricsLookupMap::const_iterator it =
      devices_metrics_.find(interface_index);
  if (it == devices_metrics_.end()) {
    SLOG(Metrics, 2) << __func__ << ": device " << interface_index
                     << " not found";
    return NULL;
  }
  return it->second.get();
}

void Metrics::AutoConnectMetricsReset(DeviceMetrics *device_metrics) {
  device_metrics->auto_connect_tries = 0;
  device_metrics->auto_connect_timer->Reset();
}

void Metrics::HourlyTimeoutHandler() {
  SLOG(Metrics, 2) << __func__;
  DeviceMetricsLookupMap::iterator it;
  for (it = devices_metrics_.begin(); it != devices_metrics_.end(); ++it) {
    if (it->second->technology != Technology::kCellular ||
        it->second->num_drops == 0)
      continue;
    SendToUMA(kMetricCellularDropsPerHour,
              it->second->num_drops,
              kMetricCellularDropsPerHourMin,
              kMetricCellularDropsPerHourMax,
              kMetricCellularDropsPerHourNumBuckets);
    it->second->num_drops = 0;
  }
  dispatcher_->PostDelayedTask(hourly_timeout_handler_.callback(),
                               kHourlyTimeoutMilliseconds);
}

void Metrics::set_library(MetricsLibraryInterface *library) {
  chromeos_metrics::TimerReporter::set_metrics_lib(library);
  library_ = library;
}

}  // namespace shill
