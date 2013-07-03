// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SYSTEM_POWER_SUPPLY_H_
#define POWER_MANAGER_POWERD_SYSTEM_POWER_SUPPLY_H_

#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include <string>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/time.h"
#include "power_manager/common/signal_callback.h"
#include "power_manager/power_supply_properties.pb.h"
#include "power_manager/powerd/system/power_supply_observer.h"
#include "power_manager/powerd/system/rolling_average.h"

typedef int gboolean;
typedef unsigned int guint;

namespace power_manager {

class Clock;
class PrefsInterface;

namespace system {

// Structures used for passing power supply info.
struct PowerStatus {
  PowerStatus()
      : line_power_on(false),
        line_power_voltage(0.0),
        line_power_current(0.0),
        battery_energy(0.0),
        battery_energy_rate(0.0),
        battery_voltage(0.0),
        battery_current(0.0),
        battery_charge(0.0),
        battery_charge_full(0.0),
        nominal_voltage(0.0),
        is_calculating_battery_time(false),
        battery_time_to_empty(0),
        battery_time_to_full(0),
        averaged_battery_time_to_empty(0),
        averaged_battery_time_to_full(0),
        battery_percentage(-1.0),
        display_battery_percentage(-1.0),
        battery_is_present(false),
        battery_below_shutdown_threshold(false),
        battery_times_are_bad(false),
        external_power(PowerSupplyProperties_ExternalPower_DISCONNECTED),
        battery_state(PowerSupplyProperties_BatteryState_NOT_PRESENT) {}

  bool line_power_on;
  std::string line_power_type;

  // Line power statistics. These may be unset even if line power is connected.
  double line_power_voltage;  // In volts.
  double line_power_current;  // In amperes.

  // Amount of energy, measured in Wh, in the battery.
  double battery_energy;

  // Amount of energy being drained from the battery, measured in W. If
  // positive, the source is being discharged, if negative it's being charged.
  double battery_energy_rate;

  // Current battery levels.
  double battery_voltage;  // In volts.
  double battery_current;  // In amperes.
  double battery_charge;   // In ampere-hours.

  // Battery full charge level in ampere-hours.
  double battery_charge_full;
  // Battery full design charge level in ampere-hours.
  double battery_charge_full_design;

  // The battery voltage used in calculating time remaining.  This may or may
  // not be the same as the instantaneous voltage |battery_voltage|, as voltage
  // levels vary over the time the battery is charged or discharged.
  double nominal_voltage;

  // Set to true when we have just transitioned states and we might have both a
  // segment of charging and discharging in the calculation. This is done to
  // signal that the time value maybe inaccurate.
  bool is_calculating_battery_time;

  // Time in seconds until the battery is considered empty, 0 for unknown.
  int64 battery_time_to_empty;
  // Time in seconds until the battery is considered full. 0 for unknown.
  int64 battery_time_to_full;

  // Averaged time in seconds until the battery is considered empty, 0 for
  // unknown.
  int64 averaged_battery_time_to_empty;
  // Average time in seconds until the battery is considered full. 0 for
  // unknown.
  int64 averaged_battery_time_to_full;

  double battery_percentage;
  double display_battery_percentage;
  bool battery_is_present;

  // Is the battery level so low that the machine should be shut down?
  bool battery_below_shutdown_threshold;

  // Does |battery_time_to_full| or |battery_time_to_empty| contain bogus
  // data?
  bool battery_times_are_bad;

  PowerSupplyProperties_ExternalPower external_power;
  PowerSupplyProperties_BatteryState battery_state;
};

struct PowerInformation {
  PowerStatus power_status;

  std::string line_power_path;
  std::string battery_path;

  // Amount of energy, measured in Wh, in the battery when it's considered
  // empty.
  double battery_energy_empty;

  // Amount of energy, measured in Wh, in the battery when it's considered full.
  double battery_energy_full;

  // Amount of energy, measured in Wh, the battery is designed to hold when it's
  // considered full.
  double battery_energy_full_design;

  std::string battery_vendor;
  std::string battery_model;
  std::string battery_serial;
  std::string battery_technology;
};

// Used to read power supply status from sysfs, e.g. whether on AC or battery,
// charge and voltage level, current, etc.
class PowerSupply {
 public:
  // Helper class for testing PowerSupply.
  class TestApi {
   public:
    explicit TestApi(PowerSupply* power_supply) : power_supply_(power_supply) {}
    ~TestApi() {}

    base::TimeDelta current_poll_delay() const {
      return power_supply_->current_poll_delay_for_testing_;
    }

    // Sets the time that will be used as "now".
    void SetCurrentTime(base::TimeTicks now);

    // Calls HandlePollTimeout() and returns true if |poll_timeout_id_| was
    // set.  Returns false if the timeout was unset.
    bool TriggerPollTimeout() WARN_UNUSED_RESULT;

   private:
    PowerSupply* power_supply_;  // not owned

    DISALLOW_COPY_AND_ASSIGN(TestApi);
  };

  // Additional time beyond |short_poll_delay_| to wait before updating the
  // status, in milliseconds.  This just ensures that the timer doesn't
  // fire before it's safe to calculate the battery time.
  static const int kCalculateBatterySlackMs;

  // Number of seconds after switching from line power with the battery
  // fully charged to battery power over which the battery's charge should
  // be reported as 100%.  Some batteries report a lower charge as soon as
  // line power is disconnected, but the percentage that's displayed to the
  // user shouldn't drop immediately.
  static const int kRetainFullChargeSec;

  PowerSupply(const base::FilePath& power_supply_path, PrefsInterface *prefs);
  ~PowerSupply();

  const PowerStatus& power_status() const { return power_status_; }

  // Initializes the object and begins polling.
  void Init();

  // Adds or removes an observer.
  void AddObserver(PowerSupplyObserver* observer);
  void RemoveObserver(PowerSupplyObserver* observer);

  // Updates |power_status_| synchronously, schedules a future poll, and
  // returns true on success.  If successful, |observers_| will be notified
  // asynchronously.
  bool RefreshImmediately();

  // Fills |info|.
  bool GetPowerInformation(PowerInformation* info);

  // On suspend, stops polling.  On resume, updates |power_status_|
  // immediately and schedules a poll for the near future.
  void SetSuspended(bool suspended);

  // Handles a power supply-related udev event.  Updates |power_status_|
  // immediately and schedules a poll for the near future.
  void HandleUdevEvent();

 private:
  friend class PowerSupplyTest;
  FRIEND_TEST(PowerSupplyTest, TestDischargingWithHysteresis);
  FRIEND_TEST(PowerSupplyTest, TestDischargingWithSuspendResume);

  base::TimeTicks GetCurrentTime() const;

  // Updates |done_calculating_battery_time_timestamp_| so PowerStatus's
  // |is_calculating_battery_time| field will be set to true until
  // |calculate_battery_time_delay_| has elapsed.
  void SetCalculatingBatteryTime();

  // Read data from power supply sysfs and fill out all fields of the
  // PowerStatus structure if possible.
  bool UpdatePowerStatus();

  // Find sysfs directories to read from.
  void GetPowerSupplyPaths();

  // Determines remaining time when charging or discharging.
  void UpdateRemainingTime(PowerStatus* status);

  // Updates the averaged values in |status| and adds the battery time
  // estimate values to the appropriate rolling averages.
  void UpdateAveragedTimes(PowerStatus* status);

  // Given the current battery time estimate, adjusts the rolling average
  // window sizes to give the desired linear tapering.
  void AdjustWindowSize(base::TimeDelta battery_time);

  // Returns the battery percentage that should be displayed for |status|.
  // |status|'s |battery_state| and |battery_percentage| fields must
  // already be set.
  double CalculateDisplayBatteryPercentage(const PowerStatus& status) const;

  // Returns true if |status|'s battery level is so low that the system
  // should be shut down.  |status|'s |battery_percentage|,
  // |battery_time_to_*|, and |line_power_on| fields must already be set.
  bool IsBatteryBelowShutdownThreshold(const PowerStatus& status) const;

  // Offsets the timestamps used in hysteresis calculations.  This is used when
  // suspending and resuming -- the time while suspended should not count toward
  // the hysteresis times.
  void AdjustHysteresisTimes(const base::TimeDelta& offset);

  // Updates |poll_timeout_id_| to call HandlePollTimeout().  Removes any
  // existing timeout.
  void SchedulePoll(bool last_poll_succeeded);

  // Handles |poll_timeout_id_| firing.  Updates |power_status_|, cancels
  // the current timeout, and schedules a new timeout.
  SIGNAL_CALLBACK_0(PowerSupply, gboolean, HandlePollTimeout);

  // Handles |notify_observers_timeout_id_| firing.  Notifies |observers_|
  // that |power_status_| has been updated and cancels the timeout.
  SIGNAL_CALLBACK_0(PowerSupply, gboolean, HandleNotifyObserversTimeout);

  // Used to read power supply-related prefs.
  PrefsInterface* prefs_;

  scoped_ptr<Clock> clock_;

  ObserverList<PowerSupplyObserver> observers_;

  // Most-recently-computed status.
  PowerStatus power_status_;

  // Paths to power supply base sysfs directory and battery and line power
  // subdirectories.
  base::FilePath power_supply_path_;
  base::FilePath line_power_path_;
  base::FilePath battery_path_;

  // These are used for using hysteresis to avoid large swings in calculated
  // remaining battery time.
  double acceptable_variance_;
  base::TimeDelta hysteresis_time_;
  bool found_acceptable_time_range_;
  double acceptable_time_;
  base::TimeDelta time_outside_of_acceptable_range_;
  base::TimeTicks last_acceptable_range_time_;
  base::TimeTicks last_poll_time_;
  base::TimeTicks discharge_start_time_;

  int64 sample_window_max_;
  int64 sample_window_min_;

  base::TimeDelta taper_time_max_;
  base::TimeDelta taper_time_min_;

  base::TimeDelta low_battery_shutdown_time_;
  double low_battery_shutdown_percent_;

  base::TimeTicks suspend_time_;
  bool is_suspended_;

  // Time at or after which it will be possible to calculate the battery's
  // time-to-full and time-to-empty.
  base::TimeTicks done_calculating_battery_time_timestamp_;

  // Rolling averages used to iron out instabilities in the time estimates.
  RollingAverage time_to_empty_average_;
  RollingAverage time_to_full_average_;

  // The fraction of full charge at which the battery can be considered "full"
  // if there is no more charging current.  Should be in the range (0, 100].
  double full_factor_;

  // Amount of time to wait before updating |power_status_| after a
  // successful update.
  base::TimeDelta poll_delay_;

  // Amount of time to wait before updating |power_status_| after an
  // unsuccessful update and to wait before recalculating battery times.
  base::TimeDelta short_poll_delay_;

  // GLib timeout ID for invoking HandlePollTimeout().
  guint poll_timeout_id_;

  // Delay used when |poll_timeout_id_| was last set.
  base::TimeDelta current_poll_delay_for_testing_;

  // GLib timeout ID for invoking HandleNotifyObserversTimeout().
  guint notify_observers_timeout_id_;

  DISALLOW_COPY_AND_ASSIGN(PowerSupply);
};

}  // namespace system
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SYSTEM_POWER_SUPPLY_H_
