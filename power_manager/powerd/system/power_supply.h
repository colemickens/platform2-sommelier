// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SYSTEM_POWER_SUPPLY_H_
#define POWER_MANAGER_POWERD_SYSTEM_POWER_SUPPLY_H_

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
        battery_percentage(-1.0),
        display_battery_percentage(-1.0),
        battery_is_present(false),
        battery_below_shutdown_threshold(false),
        external_power(PowerSupplyProperties_ExternalPower_DISCONNECTED),
        battery_state(PowerSupplyProperties_BatteryState_NOT_PRESENT) {}

  // Is a non-battery power source connected?
  bool line_power_on;

  // String read from sysfs describing the non-battery power source.
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

  // Estimated time until the battery is empty (while discharging) or full
  // (while charging).
  base::TimeDelta battery_time_to_empty;
  base::TimeDelta battery_time_to_full;

  // If discharging, estimated time until the battery is at a low-enough level
  // that the system will shut down automatically. This will be less than
  // |battery_time_to_empty| if a shutdown threshold is set.
  base::TimeDelta battery_time_to_shutdown;

  // Battery charge in the range [0.0, 100.0], i.e. |battery_charge| /
  // |battery_charge_full| * 100.0.
  double battery_percentage;

  // Battery charge in the range [0.0, 100.0] that should be displayed to
  // the user. This takes other factors into consideration, such as the
  // percentage at which point we shut down the device and the "full
  // factor".
  double display_battery_percentage;

  // Does the system have a battery?
  bool battery_is_present;

  // Is the battery level so low that the machine should be shut down?
  bool battery_below_shutdown_threshold;

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

    // Returns the time that will be used as "now".
    base::TimeTicks GetCurrentTime() const;

    // Sets the time that will be used as "now".
    void SetCurrentTime(base::TimeTicks now);

    // Advances the time by |interval|.
    void AdvanceTime(base::TimeDelta interval);

    // Calls HandlePollTimeout() and returns true if |poll_timeout_id_| was
    // set.  Returns false if the timeout was unset.
    bool TriggerPollTimeout() WARN_UNUSED_RESULT;

   private:
    PowerSupply* power_supply_;  // not owned

    DISALLOW_COPY_AND_ASSIGN(TestApi);
  };

  // Maximum number of samples of the battery current to average when
  // calculating the battery's time-to-full or -empty.
  static const int kMaxCurrentSamples;

  // Additional time beyond |current_stabilized_after_*_delay_| to wait before
  // updating the status, in milliseconds. This just ensures that the timer
  // doesn't fire before it's safe to calculate the battery time.
  static const int kCurrentStabilizedSlackMs;

  PowerSupply(const base::FilePath& power_supply_path, PrefsInterface *prefs);
  ~PowerSupply();

  const PowerStatus& power_status() const { return power_status_; }

  base::TimeTicks current_stabilized_timestamp() const {
    return current_stabilized_timestamp_;
  }

  // Useful for callers that just need an instantaneous estimate after creating
  // a PowerSupply object and don't care about smoothing.
  void clear_current_stabilized_timestamp() {
    current_stabilized_timestamp_ = base::TimeTicks();
  }

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
  // immediately.
  void HandleUdevEvent();

 private:
  friend class PowerSupplyTest;

  // Clears |current_samples_| and sets |current_stabilized_timestamp_| so that
  // the current won't be sampled again until at least |stabilized_delay| in the
  // future.
  void ResetCurrentSamples(base::TimeDelta stabilized_delay);

  // Read data from power supply sysfs and fill out all fields of the
  // PowerStatus structure if possible.
  bool UpdatePowerStatus();

  // Find sysfs directories to read from.
  void GetPowerSupplyPaths();

  // Updates |status|'s time-to-full and time-to-empty estimates or returns
  // false if estimates can't be calculated yet. Negative values are used
  // if the estimates would otherwise be extremely large (due to a very low
  // current).
  //
  // The |battery_state|, |battery_charge|, |battery_charge_full|,
  // |nominal_voltage|, and |battery_voltage| fields must already be
  // initialized.
  bool UpdateBatteryTimeEstimates(PowerStatus* status);

  // Returns true if |status|'s battery level is so low that the system
  // should be shut down.  |status|'s |battery_percentage|,
  // |battery_time_to_*|, and |line_power_on| fields must already be set.
  bool IsBatteryBelowShutdownThreshold(const PowerStatus& status) const;

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

  // True after |power_status_| has been successfully updated at least once.
  bool power_status_initialized_;

  // Paths to power supply base sysfs directory and battery and line power
  // subdirectories.
  base::FilePath power_supply_path_;
  base::FilePath line_power_path_;
  base::FilePath battery_path_;

  // Remaining battery time at which the system will shut down automatically.
  // Empty if unset.
  base::TimeDelta low_battery_shutdown_time_;

  // Remaining battery charge (as a percentage of |battery_charge_full| in the
  // range [0.0, 100.0]) at which the system will shut down automatically. 0.0
  // if unset. If both |low_battery_shutdown_time_| and this setting are
  // supplied, only |low_battery_shutdown_percent_| will take effect.
  double low_battery_shutdown_percent_;

  bool is_suspended_;

  // Amount of time to wait after startup, a power source change, or a
  // resume event before assuming that the current can be used in battery
  // time estimates.
  base::TimeDelta current_stabilized_after_startup_delay_;
  base::TimeDelta current_stabilized_after_power_source_change_delay_;
  base::TimeDelta current_stabilized_after_resume_delay_;

  // Time at which the reported current is expected to have stabilized to
  // the point where it can be recorded in |current_samples_| and the
  // battery's time-to-full or time-to-empty estimates can be updated.
  base::TimeTicks current_stabilized_timestamp_;

  // A collection of recent current readings (in amperes) used to calculate
  // time-to-full and time-to-empty estimates. Reset in response to changes
  // such as resuming from suspend or the power source being changed.
  RollingAverage current_samples_;

  // The fraction of full charge at which the battery is considered "full", in
  // the range (0.0, 1.0]. Initialized from kPowerSupplyFullFactorPref.
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
