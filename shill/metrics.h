// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_METRICS_
#define SHILL_METRICS_

#include <list>

#include <base/memory/scoped_vector.h>
#include <metrics/metrics_library.h>
#include <metrics/timer.h>

#include "shill/portal_detector.h"
#include "shill/power_manager.h"
#include "shill/refptr_types.h"
#include "shill/service.h"

namespace shill {

class WiFiService;

class Metrics {
 public:
  enum WiFiChannel {
    kWiFiChannelUndef = 0,
    kWiFiChannel2412 = 1,
    kWiFiChannel2417 = 2,
    kWiFiChannel2422 = 3,
    kWiFiChannel2427 = 4,
    kWiFiChannel2432 = 5,
    kWiFiChannel2437 = 6,
    kWiFiChannel2442 = 7,
    kWiFiChannel2447 = 8,
    kWiFiChannel2452 = 9,
    kWiFiChannel2457 = 10,
    kWiFiChannel2462 = 11,
    kWiFiChannel2467 = 12,
    kWiFiChannel2472 = 13,
    kWiFiChannel2484 = 14,

    kWiFiChannel5180 = 15,
    kWiFiChannel5200 = 16,
    kWiFiChannel5220 = 17,
    kWiFiChannel5240 = 18,
    kWiFiChannel5260 = 19,
    kWiFiChannel5280 = 20,
    kWiFiChannel5300 = 21,
    kWiFiChannel5320 = 22,

    kWiFiChannel5500 = 23,
    kWiFiChannel5520 = 24,
    kWiFiChannel5540 = 25,
    kWiFiChannel5560 = 26,
    kWiFiChannel5580 = 27,
    kWiFiChannel5600 = 28,
    kWiFiChannel5620 = 29,
    kWiFiChannel5640 = 30,
    kWiFiChannel5660 = 31,
    kWiFiChannel5680 = 32,
    kWiFiChannel5700 = 33,

    kWiFiChannel5745 = 34,
    kWiFiChannel5765 = 35,
    kWiFiChannel5785 = 36,
    kWiFiChannel5805 = 37,
    kWiFiChannel5825 = 38,

    kWiFiChannel5170 = 39,
    kWiFiChannel5190 = 40,
    kWiFiChannel5210 = 41,
    kWiFiChannel5230 = 42,

    /* NB: ignore old 11b bands 2312..2372 and 2512..2532 */
    /* NB: ignore regulated bands 4920..4980 and 5020..5160 */
    kWiFiChannelMax
  };

  enum WiFiNetworkPhyMode {
    kWiFiNetworkPhyModeUndef = 0,    // Unknown/undefined
    kWiFiNetworkPhyMode11a = 1,      // 802.11a
    kWiFiNetworkPhyMode11b = 2,      // 802.11b
    kWiFiNetworkPhyMode11g = 3,      // 802.11g
    kWiFiNetworkPhyMode11n = 4,      // 802.11n
    kWiFiNetworkPhyModeHalf = 5,     // PSB Half-width
    kWiFiNetworkPhyModeQuarter = 6,  // PSB Quarter-width
    kWiFiNetworkPhyModeTurbo = 7,    // Atheros Turbo mode

    kWiFiNetworkPhyModeMax
  };

  enum WiFiSecurity {
    kWiFiSecurityUnknown = 0,
    kWiFiSecurityNone = 1,
    kWiFiSecurityWep = 2,
    kWiFiSecurityWpa = 3,
    kWiFiSecurityRsn = 4,
    kWiFiSecurity8021x = 5,
    kWiFiSecurityPsk = 6,

    kWiFiSecurityMax
  };

  enum PortalResult {
    kPortalResultSuccess = 0,
    kPortalResultDNSFailure = 1,
    kPortalResultDNSTimeout = 2,
    kPortalResultConnectionFailure = 3,
    kPortalResultConnectionTimeout = 4,
    kPortalResultHTTPFailure = 5,
    kPortalResultHTTPTimeout = 6,
    kPortalResultContentFailure = 7,
    kPortalResultContentTimeout = 8,
    kPortalResultUnknown = 9,

    kPortalResultMax
  };

  static const char kMetricDisconnect[];
  static const int kMetricDisconnectMax;
  static const int kMetricDisconnectMin;
  static const int kMetricDisconnectNumBuckets;
  static const char kMetricNetworkChannel[];
  static const int kMetricNetworkChannelMax;
  static const char kMetricNetworkPhyMode[];
  static const int kMetricNetworkPhyModeMax;
  static const char kMetricNetworkSecurity[];
  static const int kMetricNetworkSecurityMax;
  static const char kMetricNetworkServiceErrors[];
  static const int kMetricNetworkServiceErrorsMax;
  static const char kMetricTimeOnlineSeconds[];
  static const int kMetricTimeOnlineSecondsMax;
  static const int kMetricTimeOnlineSecondsMin;
  static const int kMetricTimeOnlineSecondsNumBuckets;
  static const char kMetricTimeResumeToReadyMilliseconds[];
  static const char kMetricTimeToConfigMilliseconds[];
  static const char kMetricTimeToDropSeconds[];
  static const int kMetricTimeToDropSecondsMax;
  static const int kMetricTimeToDropSecondsMin;
  static const char kMetricTimeToJoinMilliseconds[];
  static const char kMetricTimeToOnlineMilliseconds[];
  static const char kMetricTimeToPortalMilliseconds[];
  static const int kTimerHistogramMillisecondsMax;
  static const int kTimerHistogramMillisecondsMin;
  static const int kTimerHistogramNumBuckets;

  // The number of portal detections attempted for each pass.
  // This includes both failure/timeout attempts and successful attempt
  // (if any).
  static const char kMetricPortalAttempts[];
  static const int kMetricPortalAttemptsMax;
  static const int kMetricPortalAttemptsMin;
  static const int kMetricPortalAttemptsNumBuckets;

  // The total number of portal detections attempted between the Connected
  // state and the Online state.  This includes both failure/timeout attempts
  // and the final successful attempt.
  static const char kMetricPortalAttemptsToOnline[];
  static const int kMetricPortalAttemptsToOnlineMax;
  static const int kMetricPortalAttemptsToOnlineMin;
  static const int kMetricPortalAttemptsToOnlineNumBuckets;

  // The result of the portal detection.
  static const char kMetricPortalResult[];

  static const char kMetricPowerManagerKey[];

  Metrics();
  virtual ~Metrics();

  // Converts the WiFi frequency into the associated UMA channel enumerator.
  static WiFiChannel WiFiFrequencyToChannel(uint16 frequency);

  // Converts a flimflam security string into its UMA security enumerator.
  static WiFiSecurity WiFiSecurityStringToEnum(const std::string &security);

  // Converts portal detection result to UMA portal result enumerator.
  static PortalResult PortalDetectionResultToEnum(
      const PortalDetector::Result &result);

  // Registers a service with this object so it can use the timers to track
  // state transition metrics.
  void RegisterService(const Service *service);

  // Deregisters the service from this class.  All state transition timers
  // will be removed.
  void DeregisterService(const Service *service);

  // Tracks the time it takes |service| to go from |start_state| to
  // |stop_state|.  When |stop_state| is reached, the time is sent to UMA.
  void AddServiceStateTransitionTimer(const Service *service,
                                      const std::string &histogram_name,
                                      Service::ConnectState start_state,
                                      Service::ConnectState stop_state);

  // Specializes |metric_name| for the specified |technology_id|.
  std::string GetFullMetricName(const char *metric_name,
                                Technology::Identifier technology_id);

  // Notifies this object that the default service has changed.
  // |service| is the new default service.
  virtual void NotifyDefaultServiceChanged(const Service *service);

  // Notifies this object that |service| state has changed.
  virtual void NotifyServiceStateChanged(const Service *service,
                                         Service::ConnectState new_state);

  // Notifies this object that |service| has been disconnected.
  void NotifyServiceDisconnect(const Service *service);

  // Notifies this object of a power management state change.
  void NotifyPowerStateChange(PowerManager::SuspendState new_state);

  // Sends linear histogram data to UMA.
  virtual bool SendEnumToUMA(const std::string &name, int sample, int max);

  // Send histogram data to UMA.
  virtual bool SendToUMA(const std::string &name, int sample, int min,
                         int max, int num_buckets);

 private:
  friend class MetricsTest;
  FRIEND_TEST(MetricsTest, FrequencyToChannel);
  FRIEND_TEST(MetricsTest, ServiceFailure);
  FRIEND_TEST(MetricsTest, TimeOnlineTimeToDrop);
  FRIEND_TEST(MetricsTest, TimeToConfig);
  FRIEND_TEST(MetricsTest, TimeToOnline);
  FRIEND_TEST(MetricsTest, TimeToPortal);
  FRIEND_TEST(MetricsTest, WiFiServiceChannel);
  FRIEND_TEST(MetricsTest, WiFiServicePostReady);

  typedef ScopedVector<chromeos_metrics::TimerReporter> TimerReporters;
  typedef std::list<chromeos_metrics::TimerReporter *> TimerReportersList;
  typedef std::map<Service::ConnectState, TimerReportersList>
      TimerReportersByState;
  struct ServiceMetrics {
    ServiceMetrics() : service(NULL) {}
    // The service is registered/deregistered in the Service
    // constructor/destructor, therefore there is no need to keep a ref count.
    const Service *service;
    // All TimerReporter objects are stored in |timers| which owns the objects.
    // |start_on_state| and |stop_on_state| contain pointers to the
    // TimerReporter objects and control when to start and stop the timers.
    TimerReporters timers;
    TimerReportersByState start_on_state;
    TimerReportersByState stop_on_state;
  };
  typedef std::map<const Service *, std::tr1::shared_ptr<ServiceMetrics> >
      ServiceMetricsLookupMap;

  static const uint16 kWiFiBandwidth5MHz;
  static const uint16 kWiFiBandwidth20MHz;
  static const uint16 kWiFiFrequency2412;
  static const uint16 kWiFiFrequency2472;
  static const uint16 kWiFiFrequency2484;
  static const uint16 kWiFiFrequency5170;
  static const uint16 kWiFiFrequency5180;
  static const uint16 kWiFiFrequency5230;
  static const uint16 kWiFiFrequency5240;
  static const uint16 kWiFiFrequency5320;
  static const uint16 kWiFiFrequency5500;
  static const uint16 kWiFiFrequency5700;
  static const uint16 kWiFiFrequency5745;
  static const uint16 kWiFiFrequency5825;

  void InitializeCommonServiceMetrics(const Service *service);
  void UpdateServiceStateTransitionMetrics(ServiceMetrics *service_metrics,
                                           Service::ConnectState new_state);
  void SendServiceFailure(const Service *service);

  // For unit test purposes.
  void set_library(MetricsLibraryInterface *library);
  void set_time_online_timer(chromeos_metrics::Timer *timer) {
    time_online_timer_.reset(timer);  // Passes ownership
  }
  void set_time_to_drop_timer(chromeos_metrics::Timer *timer) {
    time_to_drop_timer_.reset(timer);  // Passes ownership
  }
  void set_time_resume_to_ready_timer(chromeos_metrics::Timer *timer) {
    time_resume_to_ready_timer_.reset(timer);  // Passes ownership
  }

  // |library_| points to |metrics_library_| when shill runs normally.
  // However, in order to allow for unit testing, we point |library_| to a
  // MetricsLibraryMock object instead.
  MetricsLibrary metrics_library_;
  MetricsLibraryInterface *library_;
  ServiceMetricsLookupMap services_metrics_;
  Technology::Identifier last_default_technology_;
  bool was_online_;
  scoped_ptr<chromeos_metrics::Timer> time_online_timer_;
  scoped_ptr<chromeos_metrics::Timer> time_to_drop_timer_;
  scoped_ptr<chromeos_metrics::Timer> time_resume_to_ready_timer_;

  DISALLOW_COPY_AND_ASSIGN(Metrics);
};

}  // namespace shill

#endif  // SHILL_METRICS_
