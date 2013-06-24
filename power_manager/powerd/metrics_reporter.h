// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_METRICS_REPORTER_H_
#define POWER_MANAGER_POWERD_METRICS_REPORTER_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/time.h"
#include "power_manager/common/power_constants.h"
#include "power_manager/common/signal_callback.h"

typedef int gboolean;
typedef unsigned int guint;

class MetricsLibraryInterface;

namespace power_manager {

class PrefsInterface;

namespace policy {
class BacklightController;
}

namespace system {
struct PowerStatus;
}

// Used by Daemon to report metrics by way of Chrome.
class MetricsReporter {
 public:
  // Checks if |now| is the time to generate a new sample of a given
  // metric. Returns true if the last metric sample was generated at least
  // |interval| seconds ago (or if there is a clock jump back in time).
  static bool CheckMetricInterval(time_t now, time_t last, time_t interval);

  // Returns a copy of |enum_name| with a suffix describing |power_source|
  // appended to it.
  static std::string AppendPowerSourceToEnumName(
      const std::string& enum_name,
      PowerSource power_source);

  // Ownership of pointers remains with the caller.
  MetricsReporter(PrefsInterface* prefs,
                  MetricsLibraryInterface* metrics_lib,
                  policy::BacklightController* display_backlight_controller,
                  policy::BacklightController* keyboard_backlight_controller);
  ~MetricsReporter();

  PowerSource power_source() const { return power_source_; }

  // Initializes the object and starts various timers.
  void Init();

  // Records changes to system state.
  void HandleScreenDimmedChange(bool dimmed,
                                base::TimeTicks last_user_activity_time);
  void HandleScreenOffChange(bool off, base::TimeTicks last_user_activity_time);
  void HandlePowerSourceChange(PowerSource power_source);
  void PrepareForSuspend(const system::PowerStatus& status, base::Time now);
  void HandleResume();

  // Sends a regular (exponential) histogram sample to Chrome for transport
  // to UMA. Returns true on success. See MetricsLibrary::SendToUMA in
  // metrics/metrics_library.h for a description of the arguments.
  bool SendMetric(const std::string& name,
                  int sample,
                  int min,
                  int max,
                  int nbuckets);

  // Sends an enumeration (linear) histogram sample to Chrome for transport
  // to UMA. Returns true on success. See MetricsLibrary::SendEnumToUMA in
  // metrics/metrics_library.h for a description of the arguments.
  bool SendEnumMetric(const std::string& name,
                      int sample,
                      int max);

  // Sends a regular (exponential) histogram sample to Chrome for transport
  // to UMA. Appends the current power state to the name of the metric.
  // Returns true on success. See MetricsLibrary::SendToUMA in
  // metrics/metrics_library.h for a description of the arguments.
  bool SendMetricWithPowerSource(const std::string& name,
                                 int sample,
                                 int min,
                                 int max,
                                 int nbuckets);

  // Sends an enumeration (linear) histogram sample to Chrome for transport
  // to UMA. Appends the current power state to the name of the metric.
  // Returns true on success. See MetricsLibrary::SendEnumToUMA in
  // metrics/metrics_library.h for a description of the arguments.
  bool SendEnumMetricWithPowerSource(const std::string& name,
                                     int sample,
                                     int max);

  // Sends a metric describing a suspend attempt that didn't succeed on its
  // first attempt.  Doesn't send anything if |num_retries| is 0.
  void GenerateRetrySuspendMetric(int num_retries, int max_retries);

  // Generates UMA metrics on when leaving the idle state.
  void GenerateUserActivityMetrics();

  // Generates UMA metrics on every power event based on the current
  // power status.
  void GenerateMetricsOnPowerEvent(const system::PowerStatus& info);

  // Generates UMA metrics about the current backlight level.
  // Always returns true.
  SIGNAL_CALLBACK_0(MetricsReporter, gboolean, GenerateBacklightLevelMetric);

  // Generates a battery discharge rate UMA metric sample. Returns
  // true if a sample was sent to UMA, false otherwise.
  bool GenerateBatteryDischargeRateMetric(const system::PowerStatus& info,
                                          time_t now);

  // Sends a histogram sample containing the rate at which the battery
  // discharged while the system was suspended if the system was on battery
  // power both before suspending and after resuming.  Called by
  // GenerateMetricsOnPowerEvent().  Returns true if the sample was sent.
  bool GenerateBatteryDischargeRateWhileSuspendedMetric(
      const system::PowerStatus& status,
      base::Time now);

  // Generates a remaining battery charge and percent of full charge when charge
  // starts UMA metric sample if the current state is correct. Returns true if a
  // sample was sent to UMA, false otherwise.
  void GenerateBatteryInfoWhenChargeStartsMetric(
      const system::PowerStatus& info);

  // Calls all of the metric generation functions that need to be called at the
  // end of a session.
  void GenerateEndOfSessionMetrics(const system::PowerStatus& info,
                                   const base::TimeTicks& now,
                                   const base::TimeTicks& start);

  // Generates a remaining battery charge at end of session UMA metric
  // sample. Returns true if a sample was sent to UMA, false otherwise.
  bool GenerateBatteryRemainingAtEndOfSessionMetric(
      const system::PowerStatus& info);

  // Generates a remaining battery charge at start of session UMA metric
  // sample. Returns true if a sample was sent to UMA, false otherwise.
  bool GenerateBatteryRemainingAtStartOfSessionMetric(
      const system::PowerStatus& info);

  // Generates a number of times the ALS adjusted the backlight during a
  // session UMA metric sample. Returns true if a sample was sent to UMA, false
  // otherwise.
  bool GenerateNumberOfAlsAdjustmentsPerSessionMetric();

  // Generates a number of times the user adjusted the backlight during a
  // session UMA metric sample. Returns true if a sample was sent to UMA, false
  // otherwise.
  bool GenerateUserBrightnessAdjustmentsPerSessionMetric();

  // Generates length of session UMA metric sample. Returns true if a
  // sample was sent to UMA, false otherwise.
  bool GenerateLengthOfSessionMetric(const base::TimeTicks& now,
                                     const base::TimeTicks& start);

  // Increments the number of user sessions that have been active on the
  // current battery charge.
  void IncrementNumOfSessionsPerChargeMetric();

  // Generates number of sessions per charge UMA metric sample if the current
  // stored value is greater then 0. The stored value being 0 are spurious and
  // shouldn't be occuring, since they indicate we are on AC. Returns true if
  // a sample was sent to UMA or a 0 is silently ignored, false otherwise.
  bool GenerateNumOfSessionsPerChargeMetric();

  // Generates a metric describing power button activity.
  void GeneratePowerButtonMetric(bool down, base::TimeTicks timestamp);

 private:
  PrefsInterface* prefs_;
  MetricsLibraryInterface* metrics_lib_;
  policy::BacklightController* display_backlight_controller_;
  policy::BacklightController* keyboard_backlight_controller_;

  PowerSource power_source_;

  // False until HandlePowerSourceChange() has been called.
  bool saw_initial_power_source_;

  // GLib timeout for running GenerateBacklightLevelMetric() or 0 if unset.
  guint generate_backlight_metrics_timeout_id_;

  // Timestamp of the last generated battery discharge rate metric.
  time_t battery_discharge_rate_metric_last_;

  // Timestamp of the last time the power button was down.
  base::TimeTicks last_power_button_down_timestamp_;

  // Timestamp of the last idle event (that is, either
  // |screen_dim_timestamp_| or |screen_off_timestamp_|).
  base::TimeTicks last_idle_event_timestamp_;

  // Idle duration as of the last idle event.
  base::TimeDelta last_idle_timedelta_;

  // Timestamps of the last idle-triggered power state transitions.
  base::TimeTicks screen_dim_timestamp_;
  base::TimeTicks screen_off_timestamp_;

  // Information recorded by PrepareForSuspend() just before the system
  // suspends.  |time_before_suspend_| is intentionally base::Time rather
  // than base::TimeTicks because the latter doesn't increase while the
  // system is suspended.
  double battery_energy_before_suspend_;
  bool on_line_power_before_suspend_;
  base::Time time_before_suspend_;

  // Set by HandleResume() to indicate that
  // GenerateBatteryDischargeRateWhileSuspendedMetric() should send a
  // sample when it is next called.
  bool report_battery_discharge_rate_while_suspended_;

  DISALLOW_COPY_AND_ASSIGN(MetricsReporter);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_METRICS_REPORTER_H_
