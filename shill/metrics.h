// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_METRICS_
#define SHILL_METRICS_

#include <list>

#include <base/memory/scoped_vector.h>
#include <metrics/metrics_library.h>
#include <metrics/timer.h>

#include "shill/event_dispatcher.h"
#include "shill/ieee80211.h"
#include "shill/portal_detector.h"
#include "shill/power_manager.h"
#include "shill/refptr_types.h"
#include "shill/service.h"

namespace shill {

class WiFiMainTest;
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

  enum LinkMonitorFailure {
    kLinkMonitorMacAddressNotFound = 0,
    kLinkMonitorClientStartFailure = 1,
    kLinkMonitorTransmitFailure = 2,
    kLinkMonitorFailureThresholdReached = 3,

    kLinkMonitorFailureMax
  };

  enum WiFiStatusType {
    kStatusCodeTypeByAp,
    kStatusCodeTypeByClient,
    kStatusCodeTypeByUser,
    kStatusCodeTypeConsideredDead,
    kStatusCodeTypeMax
  };

  enum WiFiDisconnectByWhom {
    kDisconnectedByAp,
    kDisconnectedNotByAp
  };

  enum ServiceFixupProfileType {
    kMetricServiceFixupDefaultProfile,
    kMetricServiceFixupUserProfile,
    kMetricServiceFixupMax
  };

  enum TerminationActionResult {
    kTerminationActionResultSuccess,
    kTerminationActionResultFailure,
    kTerminationActionResultMax
  };

  enum TerminationActionReason {
    kTerminationActionReasonSuspend,
    kTerminationActionReasonTerminate
  };

  enum CellularDropTechnology {
    kCellularDropTechnology1Xrtt = 0,
    kCellularDropTechnologyEdge = 1,
    kCellularDropTechnologyEvdo = 2,
    kCellularDropTechnologyGprs = 3,
    kCellularDropTechnologyGsm = 4,
    kCellularDropTechnologyHspa = 5,
    kCellularDropTechnologyHspaPlus = 6,
    kCellularDropTechnologyLte = 7,
    kCellularDropTechnologyUmts = 8,
    kCellularDropTechnologyUnknown = 9,
    kCellularDropTechnologyMax
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
  static const char kMetricNetworkSignalStrength[];
  static const int kMetricNetworkSignalStrengthMin;
  static const int kMetricNetworkSignalStrengthMax;
  static const int kMetricNetworkSignalStrengthNumBuckets;
  static const char kMetricTimeOnlineSeconds[];
  static const int kMetricTimeOnlineSecondsMax;
  static const int kMetricTimeOnlineSecondsMin;
  static const int kMetricTimeOnlineSecondsNumBuckets;
  static const char kMetricTimeResumeToReadyMilliseconds[];
  static const char kMetricTimeToConfigMilliseconds[];
  static const char kMetricTimeToConnectMilliseconds[];
  static const int kMetricTimeToConnectMillisecondsMax;
  static const int kMetricTimeToConnectMillisecondsMin;
  static const int kMetricTimeToConnectMillisecondsNumBuckets;
  static const char kMetricTimeToDropSeconds[];
  static const int kMetricTimeToDropSecondsMax;
  static const int kMetricTimeToDropSecondsMin;
  static const char kMetricTimeToDisableMilliseconds[];
  static const int kMetricTimeToDisableMillisecondsMax;
  static const int kMetricTimeToDisableMillisecondsMin;
  static const int kMetricTimeToDisableMillisecondsNumBuckets;
  static const char kMetricTimeToEnableMilliseconds[];
  static const int kMetricTimeToEnableMillisecondsMax;
  static const int kMetricTimeToEnableMillisecondsMin;
  static const int kMetricTimeToEnableMillisecondsNumBuckets;
  static const char kMetricTimeToInitializeMilliseconds[];
  static const int kMetricTimeToInitializeMillisecondsMax;
  static const int kMetricTimeToInitializeMillisecondsMin;
  static const int kMetricTimeToInitializeMillisecondsNumBuckets;
  static const char kMetricTimeToJoinMilliseconds[];
  static const char kMetricTimeToOnlineMilliseconds[];
  static const char kMetricTimeToPortalMilliseconds[];
  static const char kMetricTimeToScanMilliseconds[];
  static const int kMetricTimeToScanMillisecondsMax;
  static const int kMetricTimeToScanMillisecondsMin;
  static const int kMetricTimeToScanMillisecondsNumBuckets;
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

  // LinkMonitor statistics.
  static const char kMetricLinkMonitorFailure[];
  static const char kMetricLinkMonitorResponseTimeSample[];
  static const int kMetricLinkMonitorResponseTimeSampleMin;
  static const int kMetricLinkMonitorResponseTimeSampleMax;
  static const int kMetricLinkMonitorResponseTimeSampleNumBuckets;
  static const char kMetricLinkMonitorSecondsToFailure[];
  static const int kMetricLinkMonitorSecondsToFailureMin;
  static const int kMetricLinkMonitorSecondsToFailureMax;
  static const int kMetricLinkMonitorSecondsToFailureNumBuckets;
  static const char kMetricLinkMonitorBroadcastErrorsAtFailure[];
  static const char kMetricLinkMonitorUnicastErrorsAtFailure[];
  static const int kMetricLinkMonitorErrorCountMin;
  static const int kMetricLinkMonitorErrorCountMax;
  static const int kMetricLinkMonitorErrorCountNumBuckets;

  static const char kMetricLinkClientDisconnectReason[];
  static const char kMetricLinkApDisconnectReason[];
  static const char kMetricLinkClientDisconnectType[];
  static const char kMetricLinkApDisconnectType[];

  // Shill termination action statistics.
  static const char kMetricTerminationActionTimeOnTerminate[];
  static const char kMetricTerminationActionResultOnTerminate[];
  static const char kMetricTerminationActionTimeOnSuspend[];
  static const char kMetricTerminationActionResultOnSuspend[];
  static const int kMetricTerminationActionTimeMillisecondsMax;
  static const int kMetricTerminationActionTimeMillisecondsMin;

  // WiFiService Entry Fixup.
  static const char kMetricServiceFixupEntries[];

  // Cellular specific statistics.
  static const char kMetricCellularDrop[];
  static const char kMetricCellularDropsPerHour[];
  static const int kMetricCellularDropsPerHourMax;
  static const int kMetricCellularDropsPerHourMin;
  static const int kMetricCellularDropsPerHourNumBuckets;
  static const char kMetricCellularFailureReason[];
  static const char kMetricCellularSignalStrengthBeforeDrop[];
  static const int kMetricCellularSignalStrengthBeforeDropMax;
  static const int kMetricCellularSignalStrengthBeforeDropMin;
  static const int kMetricCellularSignalStrengthBeforeDropNumBuckets;
  static const char kMetricCellularAutoConnectTries[];
  static const int kMetricCellularAutoConnectTriesMax;
  static const int kMetricCellularAutoConnectTriesMin;
  static const int kMetricCellularAutoConnectTriesNumBuckets;
  static const char kMetricCellularAutoConnectTotalTime[];
  static const int kMetricCellularAutoConnectTotalTimeMax;
  static const int kMetricCellularAutoConnectTotalTimeMin;
  static const int kMetricCellularAutoConnectTotalTimeNumBuckets;

  explicit Metrics(EventDispatcher *dispatcher);
  virtual ~Metrics();

  // Converts the WiFi frequency into the associated UMA channel enumerator.
  static WiFiChannel WiFiFrequencyToChannel(uint16 frequency);

  // Converts a flimflam security string into its UMA security enumerator.
  static WiFiSecurity WiFiSecurityStringToEnum(const std::string &security);

  // Converts portal detection result to UMA portal result enumerator.
  static PortalResult PortalDetectionResultToEnum(
      const PortalDetector::Result &result);

  // Starts this object.  Call this during initialization.
  virtual void Start();

  // Stops this object.  Call this during cleanup.
  virtual void Stop();

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

  // Notifies this object that termination actions started executing.
  void NotifyTerminationActionsStarted(TerminationActionReason reason);

  // Notifies this object that termination actions have been completed.
  // |success| is true, if the termination actions completed successfully.
  void NotifyTerminationActionsCompleted(
      TerminationActionReason reason, bool success);

  // Notifies this object of a failure in LinkMonitor.
  void NotifyLinkMonitorFailure(
      Technology::Identifier technology,
      LinkMonitorFailure failure,
      int seconds_to_failure,
      int broadcast_error_count,
      int unicast_error_count);

  // Notifies this object that LinkMonitor has added a response time sample
  // for |connection| with a value of |response_time_milliseconds|.
  void NotifyLinkMonitorResponseTimeSampleAdded(
      Technology::Identifier technology,
      int response_time_milliseconds);

  // Notifies this object of WiFi disconnect.
  void Notify80211Disconnect(WiFiDisconnectByWhom by_whom,
                             IEEE_80211::WiFiReasonCode reason);

  // Registers a device with this object so the device can use the timers to
  // track state transition metrics.
  void RegisterDevice(int interface_index,
                      Technology::Identifier technology);

  // Checks to see if the device has already been registered.
  bool IsDeviceRegistered(int interface_index,
                          Technology::Identifier technology);

  // Deregisters the device from this class.  All state transition timers
  // will be removed.
  void DeregisterDevice(int interface_index);

  // Notifies this object that a device has been initialized.
  void NotifyDeviceInitialized(int interface_index);

  // Notifies this object that a device has started the enable process.
  void NotifyDeviceEnableStarted(int interface_index);

  // Notifies this object that a device has completed the enable process.
  void NotifyDeviceEnableFinished(int interface_index);

  // Notifies this object that a device has started the disable process.
  void NotifyDeviceDisableStarted(int interface_index);

  // Notifies this object that a device has completed the disable process.
  void NotifyDeviceDisableFinished(int interface_index);

  // Notifies this object that a device has started the scanning process.
  void NotifyDeviceScanStarted(int interface_index);

  // Notifies this object that a device has completed the scanning process.
  void NotifyDeviceScanFinished(int interface_index);

  // Notifies this object that a device has started the connect process.
  void NotifyDeviceConnectStarted(int interface_index,
                                  bool is_auto_connecting);

  // Notifies this object that a device has completed the connect process.
  void NotifyDeviceConnectFinished(int interface_index);

  // Notifies this object that a cellular device has been dropped by the
  // network.
  void NotifyCellularDeviceDrop(int interface_index,
                                const std::string &network_technology,
                                uint16 signal_strength);

  // Notifies this object about a cellular device failure code.
  void NotifyCellularDeviceFailure(const Error &error);

  // Sends linear histogram data to UMA.
  virtual bool SendEnumToUMA(const std::string &name, int sample, int max);

  // Send histogram data to UMA.
  virtual bool SendToUMA(const std::string &name, int sample, int min,
                         int max, int num_buckets);

 private:
  friend class MetricsTest;
  FRIEND_TEST(MetricsTest, CellularDropsPerHour);
  FRIEND_TEST(MetricsTest, FrequencyToChannel);
  FRIEND_TEST(MetricsTest, ServiceFailure);
  FRIEND_TEST(MetricsTest, TimeOnlineTimeToDrop);
  FRIEND_TEST(MetricsTest, TimeToConfig);
  FRIEND_TEST(MetricsTest, TimeToOnline);
  FRIEND_TEST(MetricsTest, TimeToPortal);
  FRIEND_TEST(MetricsTest, TimeToScanIgnore);
  FRIEND_TEST(MetricsTest, WiFiServiceChannel);
  FRIEND_TEST(MetricsTest, WiFiServicePostReady);
  FRIEND_TEST(WiFiMainTest, GetGeolocationObjects);

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

  struct DeviceMetrics {
    DeviceMetrics() : auto_connect_tries(0), num_drops(0) {}
    Technology::Identifier technology;
    scoped_ptr<chromeos_metrics::TimerReporter> initialization_timer;
    scoped_ptr<chromeos_metrics::TimerReporter> enable_timer;
    scoped_ptr<chromeos_metrics::TimerReporter> disable_timer;
    scoped_ptr<chromeos_metrics::TimerReporter> scan_timer;
    scoped_ptr<chromeos_metrics::TimerReporter> connect_timer;
    scoped_ptr<chromeos_metrics::TimerReporter> auto_connect_timer;
    int auto_connect_tries;
    int num_drops;
  };
  typedef std::map<const int, std::tr1::shared_ptr<DeviceMetrics> >
      DeviceMetricsLookupMap;

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

  static const int kHourlyTimeoutMilliseconds;

  void InitializeCommonServiceMetrics(const Service *service);
  void UpdateServiceStateTransitionMetrics(ServiceMetrics *service_metrics,
                                           Service::ConnectState new_state);
  void SendServiceFailure(const Service *service);

  DeviceMetrics *GetDeviceMetrics (int interface_index) const;
  void AutoConnectMetricsReset(DeviceMetrics *device_metrics);

  void HourlyTimeoutHandler();

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
  void set_time_termination_actions_timer(
    chromeos_metrics::Timer *timer) {
    time_termination_actions_timer.reset(timer);  // Passes ownership
  }
  void set_time_to_scan_timer(int interface_index,
                              chromeos_metrics::TimerReporter *timer) {
    DeviceMetrics *device_metrics = GetDeviceMetrics(interface_index);
    device_metrics->scan_timer.reset(timer);  // Passes ownership
  }

  // |library_| points to |metrics_library_| when shill runs normally.
  // However, in order to allow for unit testing, we point |library_| to a
  // MetricsLibraryMock object instead.
  EventDispatcher *dispatcher_;
  MetricsLibrary metrics_library_;
  MetricsLibraryInterface *library_;
  ServiceMetricsLookupMap services_metrics_;
  Technology::Identifier last_default_technology_;
  bool was_online_;
  scoped_ptr<chromeos_metrics::Timer> time_online_timer_;
  scoped_ptr<chromeos_metrics::Timer> time_to_drop_timer_;
  scoped_ptr<chromeos_metrics::Timer> time_resume_to_ready_timer_;
  scoped_ptr<chromeos_metrics::Timer> time_termination_actions_timer;
  bool collect_bootstats_;
  DeviceMetricsLookupMap devices_metrics_;
  base::CancelableClosure hourly_timeout_handler_;

  DISALLOW_COPY_AND_ASSIGN(Metrics);
};

}  // namespace shill

#endif  // SHILL_METRICS_
