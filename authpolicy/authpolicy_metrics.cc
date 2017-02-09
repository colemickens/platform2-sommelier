// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "authpolicy/authpolicy_metrics.h"

#include <algorithm>
#include <string>
#include <utility>

#include <base/logging.h>

namespace authpolicy {

namespace {

// Prefix for all metric names.
const char* kMetricNamePrefix = "AuthPolicy.";

std::string MakeFullName(const char* metric_name) {
  return std::string(kMetricNamePrefix) + metric_name;
}

// UMA histogram parameters. The bucket layout is exponential. See
// MetricsLibrary::SendToUMA and base::Histogram for more details. |enum_value|
// is a safety parameter to make sure that the array index matches the enum
// value.
struct HistogramParams {
  int enum_value;
  const char* metric_name;
  int min_sample;
  int max_sample;
  int num_buckets;
};

// |max_sample| is the max time in milliseconds. Keep in sync with TimerType!
constexpr HistogramParams kTimerHistogramParams[TIMER_COUNT] = {
    {TIMER_NET_ADS_GPO_LIST, "TimeToRunNetAdsGpo", 0, 120000, 50},
    {TIMER_NET_ADS_INFO, "TimeToRunNetAdsInfo", 0, 120000, 50},
    {TIMER_NET_ADS_JOIN, "TimeToRunNetAdsJoin", 0, 120000, 50},
    {TIMER_NET_ADS_SEARCH, "TimeToRunNetAdsSearch", 0, 120000, 50},
    {TIMER_NET_ADS_WORKGROUP, "TimeToRunNetAdsWorkgroup", 0, 120000, 50},
    {TIMER_KINIT, "TimeToRunKinit", 0, 120000, 50},
    {TIMER_SMBCLIENT, "TimeToRunSmbclient", 0, 120000, 50},
    {TIMER_AUTHENTICATE_USER, "TimeToAuthenticateUser", 0, 600000, 50},
    {TIMER_JOIN_AD_DOMAIN, "TimeToJoinADDomain", 0, 600000, 50},
    {TIMER_REFRESH_USER_POLICY, "TimeToRefreshUserPolicy", 0, 600000, 50},
    {TIMER_REFRESH_DEVICE_POLICY, "TimeToRefreshDevicePolicy", 0, 600000, 50},
};

// UMA histogram parameters. The bucket layout is exponential. See
// Keep in sync with MetricType!
constexpr HistogramParams kMetricHistogramParams[METRIC_COUNT] = {
    {METRIC_KINIT_TRY_COUNT, "TriesOfKinit", 1, 61, 30},
    {METRIC_SMBCLIENT_TRY_COUNT, "TriesOfSmbClient", 1, 6, 7},
    {METRIC_DOWNLOAD_GPO_COUNT, "NumGposToDownload", 0, 1000, 50},
};

// D-Bus metric name plus a parameter to make sure array indices match enum
// values, see HistogramParams.
struct DBusMetricParams {
  int enum_value;
  const char* metric_name;
};

// Keep in sync with DBusCallType!
constexpr DBusMetricParams kDBusMetricParams[DBUS_CALL_COUNT] = {
    {DBUS_CALL_AUTHENTICATE_USER, "ErrorTypeOfAuthenticateUser"},
    {DBUS_CALL_JOIN_AD_DOMAIN, "ErrorTypeOfJoinADDomain"},
    {DBUS_CALL_REFRESH_USER_POLICY, "ErrorTypeOfRefreshUserPolicy"},
    {DBUS_CALL_REFRESH_DEVICE_POLICY, "ErrorTypeOfRefreshDevicePolicy"},
};

}  // namespace

ScopedTimerReporter::ScopedTimerReporter(TimerType timer_type)
    : ScopedTimerReporter(CheckedTimerType(timer_type)) {}

ScopedTimerReporter::ScopedTimerReporter(CheckedTimerType timer_type)
    : timer_(MakeFullName(kTimerHistogramParams[timer_type.value_].metric_name),
             kTimerHistogramParams[timer_type.value_].min_sample,
             kTimerHistogramParams[timer_type.value_].max_sample,
             kTimerHistogramParams[timer_type.value_].num_buckets) {
  timer_.Start();
}

ScopedTimerReporter::~ScopedTimerReporter() {
  const bool success = (timer_.Stop() && timer_.ReportMilliseconds());
  if (!success)
    LOG(WARNING) << "Timer " << timer_.histogram_name() << " failed to report.";
}

ScopedTimerReporter::CheckedTimerType::CheckedTimerType(TimerType value)
    : value_(std::min(static_cast<TimerType>(TIMER_COUNT - 1),
                      std::max(static_cast<TimerType>(0), value))) {
  DCHECK(value >= 0 && value < TIMER_COUNT);
}

// Verifies that the array order in the k*Params matches the enum_value.
template <typename T, const T* params, int n>
void CheckArrayOrder() {
  static_assert(params[n].enum_value == n, "Bad array order");
  CheckArrayOrder<T, params, n + 1>();
}
template <>
void CheckArrayOrder<HistogramParams, kTimerHistogramParams, TIMER_COUNT>() {}
template <>
void CheckArrayOrder<HistogramParams, kMetricHistogramParams, METRIC_COUNT>() {}
template <>
void CheckArrayOrder<DBusMetricParams, kDBusMetricParams, DBUS_CALL_COUNT>() {}

AuthPolicyMetrics::AuthPolicyMetrics() {
  CheckArrayOrder<HistogramParams, kTimerHistogramParams, 0>();
  CheckArrayOrder<HistogramParams, kMetricHistogramParams, 0>();
  CheckArrayOrder<DBusMetricParams, kDBusMetricParams, 0>();

  metrics_.Init();
  chromeos_metrics::TimerReporter::set_metrics_lib(&metrics_);
}

AuthPolicyMetrics::~AuthPolicyMetrics() {
  chromeos_metrics::TimerReporter::set_metrics_lib(nullptr);
}

void AuthPolicyMetrics::Report(MetricType metric_type, int sample) {
  DCHECK(metric_type >= 0 && metric_type < METRIC_COUNT);
  metrics_.SendToUMA(
      MakeFullName(kMetricHistogramParams[metric_type].metric_name),
      sample,
      kMetricHistogramParams[metric_type].min_sample,
      kMetricHistogramParams[metric_type].max_sample,
      kMetricHistogramParams[metric_type].num_buckets);
}

void AuthPolicyMetrics::ReportDBusResult(DBusCallType call_type,
                                         ErrorType error) {
  DCHECK(call_type >= 0 && call_type < DBUS_CALL_COUNT);
  metrics_.SendEnumToUMA(MakeFullName(kDBusMetricParams[call_type].metric_name),
                         static_cast<int>(error),
                         static_cast<int>(ERROR_COUNT));
}

}  // namespace authpolicy
