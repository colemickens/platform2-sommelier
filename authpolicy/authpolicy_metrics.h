// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AUTHPOLICY_AUTHPOLICY_METRICS_H_
#define AUTHPOLICY_AUTHPOLICY_METRICS_H_

#include <base/macros.h>
#include <dbus/authpolicy/dbus-constants.h>
#include <metrics/metrics_library.h>
#include <metrics/timer.h>

namespace chromeos_metrics {
class TimerReporter;
}  // namespace chromeos_metrics

namespace authpolicy {

// Timer metrics. Measure execution time of certain commands or functions. Keep
// in sync with kTimerHistogramParams!
enum TimerType {
  TIMER_NET_ADS_GPO_LIST,       // net ads gpo list.
  TIMER_NET_ADS_INFO,           // net ads info.
  TIMER_NET_ADS_JOIN,           // net ads join.
  TIMER_NET_ADS_SEARCH,         // net ads search.
  TIMER_NET_ADS_WORKGROUP,      // net ads workgroup.
  TIMER_KINIT,                  // kinit.
  TIMER_KLIST,                  // klist.
  TIMER_SMBCLIENT,              // smbclient.
  TIMER_AUTHENTICATE_USER,      // User authentication D-Bus call.
  TIMER_GET_USER_STATUS,        // User status query D-Bus call.
  TIMER_JOIN_AD_DOMAIN,         // Domain join D-Bus call.
  TIMER_REFRESH_USER_POLICY,    // User/device policy fetch D-Bus calls,
  TIMER_REFRESH_DEVICE_POLICY,  //   including the Session Manager calls.
  TIMER_COUNT,                  // Total number of timers.
  TIMER_NONE,                   // Invalid/no timer.
};

// Normal exponential metrics. Keep in sync with kMetricHistogramParams!
enum MetricType {
  METRIC_KINIT_FAILED_TRY_COUNT,      // Number of failed kinit tries.
  METRIC_SMBCLIENT_FAILED_TRY_COUNT,  // Number of failed smbclient tries.
  METRIC_DOWNLOAD_GPO_COUNT,          // Number of GPOs to download.
  METRIC_COUNT,                       // Total number of metrics.
};

// Enum metric for error types returned from D-Bus calls. Should map to
// D-Bus calls in authpolicy::AuthPolicy. Keep in sync with
// kDBusHistogramParams!
enum DBusCallType {
  DBUS_CALL_AUTHENTICATE_USER,
  DBUS_CALL_GET_USER_STATUS,
  DBUS_CALL_JOIN_AD_DOMAIN,
  DBUS_CALL_REFRESH_USER_POLICY,
  DBUS_CALL_REFRESH_DEVICE_POLICY,
  DBUS_CALL_COUNT,
};

class AuthPolicyMetrics;

// Simpler wrapper around |chromeos_metrics::TimerReporter| that starts the
// timer at construction and stops it and reports the total time at destruction.
class ScopedTimerReporter {
 public:
  explicit ScopedTimerReporter(TimerType timer_type);
  ~ScopedTimerReporter();

 private:
  // Internal fudging to make sure the range of |timer_type| is checked before
  // the array of timer parameters is accessed.
  struct CheckedTimerType {
    explicit CheckedTimerType(TimerType value);
    TimerType value_;
  };
  explicit ScopedTimerReporter(CheckedTimerType timer_type);
  chromeos_metrics::TimerReporter timer_;
  DISALLOW_COPY_AND_ASSIGN(ScopedTimerReporter);
};

// Submits UMA metrics for authpolicy. Some methods are virtual for tests.
class AuthPolicyMetrics {
 public:
  AuthPolicyMetrics();
  virtual ~AuthPolicyMetrics();

  // Report a |sample| for the given |metric_type|.
  virtual void Report(MetricType metric_type, int sample);

  // Report an |ErrorType| return value from DBus query.
  virtual void ReportDBusResult(DBusCallType call_type, ErrorType error);

 private:
  MetricsLibrary metrics_;
  DISALLOW_COPY_AND_ASSIGN(AuthPolicyMetrics);
};

}  // namespace authpolicy

#endif  // AUTHPOLICY_AUTHPOLICY_METRICS_H_
