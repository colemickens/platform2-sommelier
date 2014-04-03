// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/daemon.h"

#include <algorithm>
#include <cmath>

#include <base/bind.h>
#include <base/file_util.h>
#include <base/format_macros.h>
#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <chromeos/dbus/service_constants.h>
#include <metrics/metrics_library.h>

#include "power_manager/common/dbus_sender.h"
#include "power_manager/common/metrics_sender.h"
#include "power_manager/common/power_constants.h"
#include "power_manager/common/prefs.h"
#include "power_manager/common/util.h"
#include "power_manager/powerd/metrics_collector.h"
#include "power_manager/powerd/policy/external_backlight_controller.h"
#include "power_manager/powerd/policy/internal_backlight_controller.h"
#include "power_manager/powerd/policy/keyboard_backlight_controller.h"
#include "power_manager/powerd/policy/state_controller.h"
#include "power_manager/powerd/policy/suspender.h"
#include "power_manager/powerd/system/ambient_light_sensor.h"
#include "power_manager/powerd/system/audio_client.h"
#include "power_manager/powerd/system/display/display_power_setter.h"
#include "power_manager/powerd/system/display/display_watcher.h"
#include "power_manager/powerd/system/input.h"
#include "power_manager/powerd/system/internal_backlight.h"
#include "power_manager/powerd/system/udev.h"
#include "power_manager/proto_bindings/policy.pb.h"
#include "power_manager/proto_bindings/power_supply_properties.pb.h"

namespace power_manager {

namespace {

// Path to file that's touched before the system suspends and unlinked
// after it resumes. Used by crash-reporter to avoid reporting unclean
// shutdowns that occur while the system is suspended (i.e. probably due to
// the battery charge reaching zero).
const char kSuspendedStatePath[] = "/var/lib/power_manager/powerd_suspended";

// Strings for states that powerd cares about from the session manager's
// SessionStateChanged signal.
const char kSessionStarted[] = "started";

// Path containing the number of wakeup events.
const char kWakeupCountPath[] = "/sys/power/wakeup_count";

// Program used to run code as root.
const char kSetuidHelperPath[] = "/usr/bin/powerd_setuid_helper";

// File that's created once the out-of-box experience has been completed.
const char kOobeCompletedPath[] = "/home/chronos/.oobe_completed";

// Maximum amount of time to wait for responses to D-Bus method calls to other
// processes.
const int kSessionManagerDBusTimeoutMs = 3000;
const int kUpdateEngineDBusTimeoutMs = 3000;

// Copies fields from |status| into |proto|.
void CopyPowerStatusToProtocolBuffer(const system::PowerStatus& status,
                                     PowerSupplyProperties* proto) {
  DCHECK(proto);
  proto->Clear();
  proto->set_external_power(status.external_power);
  proto->set_battery_state(status.battery_state);
  proto->set_battery_percent(status.display_battery_percentage);
  // Show the user the time until powerd will shut down the system automatically
  // rather than the time until the battery is completely empty.
  proto->set_battery_time_to_empty_sec(
      status.battery_time_to_shutdown.InSeconds());
  proto->set_battery_time_to_full_sec(
      status.battery_time_to_full.InSeconds());
  proto->set_is_calculating_battery_time(status.is_calculating_battery_time);
  if (status.battery_state == PowerSupplyProperties_BatteryState_FULL ||
      status.battery_state == PowerSupplyProperties_BatteryState_CHARGING) {
    proto->set_battery_discharge_rate(-status.battery_energy_rate);
  } else {
    proto->set_battery_discharge_rate(status.battery_energy_rate);
  }
}

// Returns a string describing the battery status from |status|.
std::string GetPowerStatusBatteryDebugString(
    const system::PowerStatus& status) {
  if (!status.battery_is_present)
    return std::string();

  std::string output;
  switch (status.external_power) {
    case PowerSupplyProperties_ExternalPower_AC:
    case PowerSupplyProperties_ExternalPower_USB:
    case PowerSupplyProperties_ExternalPower_ORIGINAL_SPRING_CHARGER: {
      const char* type =
          (status.external_power == PowerSupplyProperties_ExternalPower_AC ||
           status.external_power ==
           PowerSupplyProperties_ExternalPower_ORIGINAL_SPRING_CHARGER) ? "AC" :
          "USB";

      std::string kernel_type = status.line_power_type;
      if (status.external_power ==
          PowerSupplyProperties_ExternalPower_ORIGINAL_SPRING_CHARGER)
        kernel_type += "-orig-spring";

      output = base::StringPrintf("On %s (%s", type, kernel_type.c_str());
      if (status.line_power_current || status.line_power_voltage) {
        output += base::StringPrintf(", %.3fA at %.1fV",
            status.line_power_current, status.line_power_voltage);
      }
      output += ") with battery at ";
      break;
    }
    case PowerSupplyProperties_ExternalPower_DISCONNECTED:
      output = "On battery at ";
      break;
  }

  long rounded_actual = lround(status.battery_percentage);
  long rounded_display = lround(status.display_battery_percentage);
  output += base::StringPrintf("%ld%%", rounded_actual);
  if (rounded_actual != rounded_display)
    output += base::StringPrintf(" (displayed as %ld%%)", rounded_display);
  output += base::StringPrintf(", %.3f/%.3fAh at %.3fA", status.battery_charge,
      status.battery_charge_full, status.battery_current);

  switch (status.battery_state) {
    case PowerSupplyProperties_BatteryState_FULL:
      output += ", full";
      break;
    case PowerSupplyProperties_BatteryState_CHARGING:
      if (status.battery_time_to_full >= base::TimeDelta()) {
        output += ", " + util::TimeDeltaToString(status.battery_time_to_full) +
            " until full";
        if (status.is_calculating_battery_time)
          output += " (calculating)";
      }
      break;
    case PowerSupplyProperties_BatteryState_DISCHARGING:
      if (status.battery_time_to_empty >= base::TimeDelta()) {
        output += ", " + util::TimeDeltaToString(status.battery_time_to_empty) +
            " until empty";
        if (status.is_calculating_battery_time) {
          output += " (calculating)";
        } else if (status.battery_time_to_shutdown !=
                   status.battery_time_to_empty) {
          output += base::StringPrintf(" (%s until shutdown)",
              util::TimeDeltaToString(status.battery_time_to_shutdown).c_str());
        }
      }
      break;
    case PowerSupplyProperties_BatteryState_NOT_PRESENT:
      break;
  }

  return output;
}

// Passes |method_call| to |handler| and passes the response to
// |response_sender|. If |handler| returns NULL, an empty response is created
// and sent.
void HandleSynchronousDBusMethodCall(
    base::Callback<scoped_ptr<dbus::Response>(dbus::MethodCall*)> handler,
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  scoped_ptr<dbus::Response> response = handler.Run(method_call);
  if (!response)
    response = dbus::Response::FromMethodCall(method_call);
  response_sender.Run(response.Pass());
}

// Creates a new "not supported" reply to |method_call|.
scoped_ptr<dbus::Response> CreateNotSupportedError(
    dbus::MethodCall* method_call,
    std::string message) {
  return scoped_ptr<dbus::Response>(
      dbus::ErrorResponse::FromMethodCall(
          method_call, DBUS_ERROR_NOT_SUPPORTED, message));
}

// Creates a new "invalid args" reply to |method_call|.
scoped_ptr<dbus::Response> CreateInvalidArgsError(
    dbus::MethodCall* method_call,
    std::string message) {
  return scoped_ptr<dbus::Response>(
      dbus::ErrorResponse::FromMethodCall(
          method_call, DBUS_ERROR_INVALID_ARGS, message));
}

// Runs powerd_setuid_helper. |action| is passed via --action.  If
// |additional_args| is non-empty, it will be appended to the command. If
// |wait_for_completion| is true, this function will block until the helper
// finishes and return the helper's exit code; otherwise it will return 0
// immediately.
int RunSetuidHelper(const std::string& action,
                    const std::string& additional_args,
                    bool wait_for_completion) {
  std::string command = kSetuidHelperPath + std::string(" --action=" + action);
  if (!additional_args.empty())
    command += " " + additional_args;
  if (wait_for_completion) {
    return util::Run(command.c_str());
  } else {
    util::Launch(command.c_str());
    return 0;
  }
}

}  // namespace

// Performs actions requested by |state_controller_|.  The reason that
// this is a nested class of Daemon rather than just being implented as
// part of Daemon is to avoid method naming conflicts.
class Daemon::StateControllerDelegate
    : public policy::StateController::Delegate {
 public:
  explicit StateControllerDelegate(Daemon* daemon) : daemon_(daemon) {}
  virtual ~StateControllerDelegate() {
    daemon_ = NULL;
  }

  // Overridden from policy::StateController::Delegate:
  virtual bool IsUsbInputDeviceConnected() OVERRIDE {
    return daemon_->input_->IsUSBInputDeviceConnected();
  }

  virtual bool IsOobeCompleted() OVERRIDE {
    return base::PathExists(base::FilePath(kOobeCompletedPath));
  }

  virtual bool IsHdmiAudioActive() OVERRIDE {
    return daemon_->audio_client_->hdmi_active();
  }

  virtual bool IsHeadphoneJackPlugged() OVERRIDE {
    return daemon_->audio_client_->headphone_jack_plugged();
  }

  virtual LidState QueryLidState() OVERRIDE {
    return daemon_->input_->QueryLidState();
  }

  virtual void DimScreen() OVERRIDE {
    daemon_->SetBacklightsDimmedForInactivity(true);
  }

  virtual void UndimScreen() OVERRIDE {
    daemon_->SetBacklightsDimmedForInactivity(false);
  }

  virtual void TurnScreenOff() OVERRIDE {
    daemon_->SetBacklightsOffForInactivity(true);
  }

  virtual void TurnScreenOn() OVERRIDE {
    daemon_->SetBacklightsOffForInactivity(false);
  }

  virtual void LockScreen() OVERRIDE {
    dbus::MethodCall method_call(
        login_manager::kSessionManagerInterface,
        login_manager::kSessionManagerLockScreen);
    scoped_ptr<dbus::Response> response(
        daemon_->session_manager_dbus_proxy_->CallMethodAndBlock(
            &method_call, kSessionManagerDBusTimeoutMs));
  }

  virtual void Suspend() OVERRIDE {
    daemon_->Suspend(false, 0);
  }

  virtual void StopSession() OVERRIDE {
    // This session manager method takes a string argument, although it
    // doesn't currently do anything with it.
    dbus::MethodCall method_call(
        login_manager::kSessionManagerInterface,
        login_manager::kSessionManagerStopSession);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString("");
    scoped_ptr<dbus::Response> response(
        daemon_->session_manager_dbus_proxy_->CallMethodAndBlock(
            &method_call, kSessionManagerDBusTimeoutMs));
  }

  virtual void ShutDown() OVERRIDE {
    daemon_->ShutDown(SHUTDOWN_MODE_POWER_OFF,
                      SHUTDOWN_REASON_STATE_TRANSITION);
  }

  virtual void UpdatePanelForDockedMode(bool docked) OVERRIDE {
    daemon_->SetBacklightsDocked(docked);
  }

  virtual void EmitIdleActionImminent() OVERRIDE {
    daemon_->dbus_sender_->EmitBareSignal(kIdleActionImminentSignal);
  }

  virtual void EmitIdleActionDeferred() OVERRIDE {
    daemon_->dbus_sender_->EmitBareSignal(kIdleActionDeferredSignal);
  }

  virtual void ReportUserActivityMetrics() OVERRIDE {
    daemon_->metrics_collector_->GenerateUserActivityMetrics();
  }

 private:
  Daemon* daemon_;  // weak

  DISALLOW_COPY_AND_ASSIGN(StateControllerDelegate);
};

// Performs actions on behalf of Suspender.
class Daemon::SuspenderDelegate : public policy::Suspender::Delegate {
 public:
  SuspenderDelegate(Daemon* daemon) : daemon_(daemon) {}
  virtual ~SuspenderDelegate() {}

  // Delegate implementation:
  virtual bool IsLidClosed() OVERRIDE {
    return daemon_->input_->QueryLidState() == LID_CLOSED;
  }

  virtual bool GetWakeupCount(uint64* wakeup_count) OVERRIDE {
    DCHECK(wakeup_count);
    base::FilePath path(kWakeupCountPath);
    std::string buf;
    if (base::ReadFileToString(path, &buf)) {
      TrimWhitespaceASCII(buf, TRIM_TRAILING, &buf);
      if (base::StringToUint64(buf, wakeup_count))
        return true;

      LOG(ERROR) << "Could not parse wakeup count from \"" << buf << "\"";
    } else {
      LOG(ERROR) << "Could not read " << kWakeupCountPath;
    }
    return false;
  }

  virtual void PrepareForSuspendAnnouncement() OVERRIDE {
    // When going to suspend, notify the backlight controller so it can turn
    // the backlight off and tell the kernel to resume the current level
    // after resuming.  This must occur before Chrome is told that the system
    // is going to suspend (Chrome turns the display back on while leaving
    // the backlight off).
    daemon_->SetBacklightsSuspended(true);
  }

  virtual void HandleCanceledSuspendAnnouncement() OVERRIDE {
    // Undo the earlier call.
    daemon_->SetBacklightsSuspended(false);
    SendPowerStateChangedSignal("on");
  }

  virtual void PrepareForSuspend() {
    daemon_->PrepareForSuspend();
    SendPowerStateChangedSignal("mem");
  }

  virtual SuspendResult Suspend(uint64 wakeup_count,
                                bool wakeup_count_valid,
                                base::TimeDelta duration) OVERRIDE {
    std::string args;
    if (wakeup_count_valid) {
      args += base::StringPrintf(" --suspend_wakeup_count_valid"
                                 " --suspend_wakeup_count %" PRIu64,
                                 wakeup_count);
    }
    if (duration != base::TimeDelta()) {
      args += base::StringPrintf(" --suspend_duration %" PRId64,
                                 duration.InSeconds());
    }

    // These exit codes are defined in scripts/powerd_suspend.
    switch (RunSetuidHelper("suspend", args, true)) {
      case 0:
        return SUSPEND_SUCCESSFUL;
      case 1:
        return SUSPEND_FAILED;
      case 2:  // Canceled before write to wakeup_count.
        return SUSPEND_CANCELED;
      case 3:  // Canceled after write to wakeup_count.
        return SUSPEND_CANCELED;
      default:
        LOG(ERROR) << "Treating unexpected exit code as suspend failure";
        return SUSPEND_FAILED;
    }
  }

  virtual void HandleSuspendAttemptCompletion(
      bool suspend_was_successful,
      int num_suspend_attempts) OVERRIDE {
    SendPowerStateChangedSignal("on");
    daemon_->HandleSuspendAttemptCompletion(suspend_was_successful,
                                            num_suspend_attempts);
  }

  virtual void HandleCanceledSuspendRequest(int num_suspend_attempts) {
    daemon_->metrics_collector_->HandleCanceledSuspendRequest(
        num_suspend_attempts);
  }

  virtual void ShutDownForFailedSuspend() OVERRIDE {
    daemon_->ShutDown(SHUTDOWN_MODE_POWER_OFF, SHUTDOWN_REASON_SUSPEND_FAILED);
  }

  virtual void ShutDownForDarkResume() OVERRIDE {
    daemon_->ShutDown(SHUTDOWN_MODE_POWER_OFF, SHUTDOWN_REASON_DARK_RESUME);
  }

 private:
  // TODO(derat): Remove this: http://crbug.com/359619.
  void SendPowerStateChangedSignal(const std::string& power_state) {
    dbus::Signal signal(kPowerManagerInterface, kPowerStateChanged);
    dbus::MessageWriter writer(&signal);
    writer.AppendString(power_state);
    daemon_->powerd_dbus_object_->SendSignal(&signal);
  }

  Daemon* daemon_;  // weak

  DISALLOW_COPY_AND_ASSIGN(SuspenderDelegate);
};

Daemon::Daemon(const base::FilePath& read_write_prefs_dir,
               const base::FilePath& read_only_prefs_dir,
               const base::FilePath& run_dir)
    : prefs_(new Prefs),
      powerd_dbus_object_(NULL),
      chrome_dbus_proxy_(NULL),
      session_manager_dbus_proxy_(NULL),
      cras_dbus_proxy_(NULL),
      update_engine_dbus_proxy_(NULL),
      state_controller_delegate_(new StateControllerDelegate(this)),
      dbus_sender_(new DBusSender),
      display_watcher_(new system::DisplayWatcher),
      display_power_setter_(new system::DisplayPowerSetter),
      udev_(new system::Udev),
      input_(new system::Input),
      state_controller_(new policy::StateController),
      input_controller_(new policy::InputController),
      audio_client_(new system::AudioClient),
      peripheral_battery_watcher_(new system::PeripheralBatteryWatcher),
      power_supply_(new system::PowerSupply),
      dark_resume_policy_(new policy::DarkResumePolicy),
      suspender_delegate_(new SuspenderDelegate(this)),
      suspender_(new policy::Suspender),
      metrics_collector_(new MetricsCollector),
      shutting_down_(false),
      run_dir_(run_dir),
      session_state_(SESSION_STOPPED),
      state_controller_initialized_(false),
      created_suspended_state_file_(false),
      lock_vt_before_suspend_(false),
      log_suspend_with_mosys_eventlog_(false) {
  scoped_ptr<MetricsLibrary> metrics_lib(new MetricsLibrary);
  metrics_lib->Init();
  metrics_sender_.reset(
      new MetricsSender(metrics_lib.PassAs<MetricsLibraryInterface>()));

  CHECK(prefs_->Init(util::GetPrefPaths(
      base::FilePath(read_write_prefs_dir),
      base::FilePath(read_only_prefs_dir))));
  power_supply_->AddObserver(this);
  audio_client_->AddObserver(this);
}

Daemon::~Daemon() {
  audio_client_->RemoveObserver(this);
  if (display_backlight_controller_)
    display_backlight_controller_->RemoveObserver(this);
  power_supply_->RemoveObserver(this);
}

void Daemon::Init() {
  InitDBus();
  CHECK(udev_->Init());

  if (BoolPrefIsTrue(kHasAmbientLightSensorPref)) {
    light_sensor_.reset(new system::AmbientLightSensor);
    light_sensor_->Init();
  }

  display_watcher_->Init(udev_.get());
  display_power_setter_->Init(chrome_dbus_proxy_);
  if (BoolPrefIsTrue(kExternalDisplayOnlyPref)) {
    display_backlight_controller_.reset(
        new policy::ExternalBacklightController);
    static_cast<policy::ExternalBacklightController*>(
        display_backlight_controller_.get())->Init(display_watcher_.get(),
                                                   display_power_setter_.get());
  } else {
    display_backlight_.reset(new system::InternalBacklight);
    if (!display_backlight_->Init(base::FilePath(kInternalBacklightPath),
                                  kInternalBacklightPattern)) {
      LOG(ERROR) << "Cannot initialize display backlight";
      display_backlight_.reset();
    } else {
      display_backlight_controller_.reset(
          new policy::InternalBacklightController);
      static_cast<policy::InternalBacklightController*>(
         display_backlight_controller_.get())->Init(
             display_backlight_.get(), prefs_.get(), light_sensor_.get(),
             display_power_setter_.get());
    }
  }
  if (display_backlight_controller_)
    display_backlight_controller_->AddObserver(this);

  if (BoolPrefIsTrue(kHasKeyboardBacklightPref)) {
    if (!light_sensor_.get()) {
      LOG(ERROR) << "Keyboard backlight requires ambient light sensor";
    } else {
      keyboard_backlight_.reset(new system::InternalBacklight);
      if (!keyboard_backlight_->Init(base::FilePath(kKeyboardBacklightPath),
                                     kKeyboardBacklightPattern)) {
        LOG(ERROR) << "Cannot initialize keyboard backlight";
        keyboard_backlight_.reset();
      } else {
        keyboard_backlight_controller_.reset(
            new policy::KeyboardBacklightController);
        keyboard_backlight_controller_->Init(
            keyboard_backlight_.get(), prefs_.get(), light_sensor_.get(),
            display_backlight_controller_.get());
      }
    }
  }

  prefs_->GetBool(kLockVTBeforeSuspendPref, &lock_vt_before_suspend_);
  prefs_->GetBool(kMosysEventlogPref, &log_suspend_with_mosys_eventlog_);

  power_supply_->Init(base::FilePath(kPowerStatusPath),
                      prefs_.get(), udev_.get());
  power_supply_->RefreshImmediately();

  metrics_collector_->Init(prefs_.get(), display_backlight_controller_.get(),
                           keyboard_backlight_controller_.get(),
                           power_supply_->power_status());

  OnPowerStatusUpdate();

  dark_resume_policy_->Init(power_supply_.get(), prefs_.get());
  suspender_->Init(suspender_delegate_.get(), dbus_sender_.get(),
                   dark_resume_policy_.get(), prefs_.get());

  CHECK(input_->Init(prefs_.get(), udev_.get()));
  input_controller_->Init(input_.get(), this, display_watcher_.get(),
                          dbus_sender_.get(), prefs_.get());

  const PowerSource power_source =
      power_supply_->power_status().line_power_on ? POWER_AC : POWER_BATTERY;
  state_controller_->Init(state_controller_delegate_.get(), prefs_.get(),
                          power_source, input_->QueryLidState());
  state_controller_initialized_ = true;

  audio_client_->Init(cras_dbus_proxy_);
  peripheral_battery_watcher_->Init(dbus_sender_.get());
}

bool Daemon::BoolPrefIsTrue(const std::string& name) const {
  bool value = false;
  return prefs_->GetBool(name, &value) && value;
}

void Daemon::AdjustKeyboardBrightness(int direction) {
  if (!keyboard_backlight_controller_)
    return;

  if (direction > 0)
    keyboard_backlight_controller_->IncreaseUserBrightness();
  else if (direction < 0)
    keyboard_backlight_controller_->DecreaseUserBrightness(
        true /* allow_off */);
}

void Daemon::SendBrightnessChangedSignal(
    double brightness_percent,
    policy::BacklightController::BrightnessChangeCause cause,
    const std::string& signal_name) {
  dbus::Signal signal(kPowerManagerInterface, signal_name);
  dbus::MessageWriter writer(&signal);
  writer.AppendInt32(round(brightness_percent));
  writer.AppendBool(cause ==
      policy::BacklightController::BRIGHTNESS_CHANGE_USER_INITIATED);
  powerd_dbus_object_->SendSignal(&signal);
}

void Daemon::OnBrightnessChanged(
    double brightness_percent,
    policy::BacklightController::BrightnessChangeCause cause,
    policy::BacklightController* source) {
  if (source == display_backlight_controller_ &&
      display_backlight_controller_) {
    SendBrightnessChangedSignal(brightness_percent, cause,
                                kBrightnessChangedSignal);
  } else if (source == keyboard_backlight_controller_.get() &&
             keyboard_backlight_controller_) {
    SendBrightnessChangedSignal(brightness_percent, cause,
                                kKeyboardBrightnessChangedSignal);
  } else {
    NOTREACHED() << "Received a brightness change callback from an unknown "
                 << "backlight controller";
  }
}

void Daemon::PrepareForSuspend() {
  // This command is run synchronously to ensure that it finishes before the
  // system is suspended.
  // TODO(derat): Remove this once it's logged by the kernel:
  // http://crosbug.com/p/16132
  if (log_suspend_with_mosys_eventlog_)
    RunSetuidHelper("mosys_eventlog", "--mosys_eventlog_code=0xa7", true);

  // Do not let suspend change the console terminal.
  if (lock_vt_before_suspend_)
    RunSetuidHelper("lock_vt", "", true);

  // Touch a file that crash-reporter can inspect later to determine
  // whether the system was suspended while an unclean shutdown occurred.
  // If the file already exists, assume that crash-reporter hasn't seen it
  // yet and avoid unlinking it after resume.
  created_suspended_state_file_ = false;
  const base::FilePath kStatePath(kSuspendedStatePath);
  if (!base::PathExists(kStatePath)) {
    if (file_util::WriteFile(kStatePath, NULL, 0) == 0)
      created_suspended_state_file_ = true;
    else
      LOG(WARNING) << "Unable to create " << kSuspendedStatePath;
  }

  power_supply_->SetSuspended(true);
  audio_client_->MuteSystem();
  metrics_collector_->PrepareForSuspend();
}

void Daemon::HandleSuspendAttemptCompletion(bool suspend_was_successful,
                                            int num_suspend_attempts) {
  SetBacklightsSuspended(false);
  audio_client_->RestoreMutedState();
  power_supply_->SetSuspended(false);

  // Allow virtual terminal switching again.
  if (lock_vt_before_suspend_)
    RunSetuidHelper("unlock_vt", "", true);

  if (created_suspended_state_file_) {
    if (!base::DeleteFile(base::FilePath(kSuspendedStatePath), false))
      LOG(ERROR) << "Failed to delete " << kSuspendedStatePath;
  }

  if (suspend_was_successful)
    metrics_collector_->HandleResume(num_suspend_attempts);

  // TODO(derat): Remove this once it's logged by the kernel:
  // http://crosbug.com/p/16132
  if (log_suspend_with_mosys_eventlog_)
    RunSetuidHelper("mosys_eventlog", "--mosys_eventlog_code=0xa8", false);

  // StateController may synchronously trigger another suspend attempt if the
  // lid is still closed. Notify it last to ensure that all other cleanup is
  // already done.
  state_controller_->HandleResume();
}

void Daemon::HandleLidClosed() {
  if (state_controller_initialized_)
    state_controller_->HandleLidStateChange(LID_CLOSED);
}

void Daemon::HandleLidOpened() {
  suspender_->HandleLidOpened();
  if (state_controller_initialized_)
    state_controller_->HandleLidStateChange(LID_OPEN);
}

void Daemon::HandlePowerButtonEvent(ButtonState state) {
  metrics_collector_->HandlePowerButtonEvent(state);
  if (state == BUTTON_DOWN && display_backlight_controller_)
    display_backlight_controller_->HandlePowerButtonPress();
}

void Daemon::DeferInactivityTimeoutForVT2() {
  LOG(INFO) << "Reporting synthetic user activity since VT2 is active";
  state_controller_->HandleUserActivity();
}

void Daemon::ShutDownForPowerButtonWithNoDisplay() {
  LOG(INFO) << "Shutting down due to power button press while no display is "
            << "connected";
  metrics_collector_->HandlePowerButtonEvent(BUTTON_DOWN);
  ShutDown(SHUTDOWN_MODE_POWER_OFF, SHUTDOWN_REASON_USER_REQUEST);
}

void Daemon::HandleMissingPowerButtonAcknowledgment() {
  LOG(INFO) << "Didn't receive power button acknowledgment from Chrome";
  util::Launch("sync");
}

void Daemon::ReportPowerButtonAcknowledgmentDelay(base::TimeDelta delay) {
  metrics_collector_->SendPowerButtonAcknowledgmentDelayMetric(delay);
}

void Daemon::OnAudioStateChange(bool active) {
  // |state_controller_| needs to be ready at this point -- since notifications
  // only arrive when the audio state changes, skipping any is unsafe.
  CHECK(state_controller_initialized_);
  state_controller_->HandleAudioStateChange(active);
}

void Daemon::OnPowerStatusUpdate() {
  const system::PowerStatus& status = power_supply_->power_status();
  if (status.battery_is_present)
    LOG(INFO) << GetPowerStatusBatteryDebugString(status);

  metrics_collector_->HandlePowerStatusUpdate(status);

  const PowerSource power_source =
      status.line_power_on ? POWER_AC : POWER_BATTERY;
  if (display_backlight_controller_)
    display_backlight_controller_->HandlePowerSourceChange(power_source);
  if (keyboard_backlight_controller_)
    keyboard_backlight_controller_->HandlePowerSourceChange(power_source);
  if (state_controller_initialized_)
    state_controller_->HandlePowerSourceChange(power_source);

  if (status.battery_is_present && status.battery_below_shutdown_threshold) {
    LOG(INFO) << "Shutting down due to low battery ("
              << base::StringPrintf("%0.2f", status.battery_percentage) << "%, "
              << util::TimeDeltaToString(status.battery_time_to_empty)
              << " until empty, "
              << base::StringPrintf(
                     "%0.3f", status.observed_battery_charge_rate)
              << "A observed charge rate)";
    ShutDown(SHUTDOWN_MODE_POWER_OFF, SHUTDOWN_REASON_LOW_BATTERY);
  }

  PowerSupplyProperties protobuf;
  CopyPowerStatusToProtocolBuffer(status, &protobuf);
  dbus_sender_->EmitSignalWithProtocolBuffer(kPowerSupplyPollSignal, protobuf);
}

void Daemon::InitDBus() {
  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  bus_ = new dbus::Bus(options);
  CHECK(bus_->Connect());

  chrome_dbus_proxy_ = bus_->GetObjectProxy(
      chromeos::kLibCrosServiceName,
      dbus::ObjectPath(chromeos::kLibCrosServicePath));
  chrome_dbus_proxy_->WaitForServiceToBeAvailable(
      base::Bind(&Daemon::HandleChromeAvailableOrRestarted,
                 base::Unretained(this)));

  session_manager_dbus_proxy_ = bus_->GetObjectProxy(
      login_manager::kSessionManagerServiceName,
      dbus::ObjectPath(login_manager::kSessionManagerServicePath));
  session_manager_dbus_proxy_->WaitForServiceToBeAvailable(
      base::Bind(&Daemon::HandleSessionManagerAvailableOrRestarted,
                 base::Unretained(this)));
  session_manager_dbus_proxy_->ConnectToSignal(
      login_manager::kSessionManagerInterface,
      login_manager::kSessionStateChangedSignal,
      base::Bind(&Daemon::HandleSessionStateChangedSignal,
                 base::Unretained(this)),
      base::Bind(&Daemon::HandleDBusSignalConnected, base::Unretained(this)));

  cras_dbus_proxy_ = bus_->GetObjectProxy(
      cras::kCrasServiceName,
      dbus::ObjectPath(cras::kCrasServicePath));
  cras_dbus_proxy_->WaitForServiceToBeAvailable(
      base::Bind(&Daemon::HandleCrasAvailableOrRestarted,
                 base::Unretained(this)));
  cras_dbus_proxy_->ConnectToSignal(
      cras::kCrasControlInterface,
      cras::kNodesChanged,
      base::Bind(&Daemon::HandleCrasNodesChangedSignal, base::Unretained(this)),
      base::Bind(&Daemon::HandleDBusSignalConnected, base::Unretained(this)));
  cras_dbus_proxy_->ConnectToSignal(
      cras::kCrasControlInterface,
      cras::kActiveOutputNodeChanged,
      base::Bind(&Daemon::HandleCrasActiveOutputNodeChangedSignal,
                 base::Unretained(this)),
      base::Bind(&Daemon::HandleDBusSignalConnected, base::Unretained(this)));
  cras_dbus_proxy_->ConnectToSignal(
      cras::kCrasControlInterface,
      cras::kNumberOfActiveStreamsChanged,
      base::Bind(&Daemon::HandleCrasNumberOfActiveStreamsChanged,
                 base::Unretained(this)),
      base::Bind(&Daemon::HandleDBusSignalConnected, base::Unretained(this)));

  update_engine_dbus_proxy_ = bus_->GetObjectProxy(
      update_engine::kUpdateEngineServiceName,
      dbus::ObjectPath(update_engine::kUpdateEngineServicePath));
  update_engine_dbus_proxy_->WaitForServiceToBeAvailable(
      base::Bind(&Daemon::HandleUpdateEngineAvailableOrRestarted,
                 base::Unretained(this)));
  update_engine_dbus_proxy_->ConnectToSignal(
      update_engine::kUpdateEngineInterface,
      update_engine::kStatusUpdate,
      base::Bind(&Daemon::HandleUpdateEngineStatusUpdateSignal,
                 base::Unretained(this)),
      base::Bind(&Daemon::HandleDBusSignalConnected, base::Unretained(this)));

  powerd_dbus_object_ = bus_->GetExportedObject(
      dbus::ObjectPath(kPowerManagerServicePath));
  ExportDBusMethod(kRequestShutdownMethod,
                   &Daemon::HandleRequestShutdownMethod);
  ExportDBusMethod(kRequestRestartMethod,
                   &Daemon::HandleRequestRestartMethod);
  ExportDBusMethod(kRequestSuspendMethod,
                   &Daemon::HandleRequestSuspendMethod);
  ExportDBusMethod(kDecreaseScreenBrightnessMethod,
                   &Daemon::HandleDecreaseScreenBrightnessMethod);
  ExportDBusMethod(kIncreaseScreenBrightnessMethod,
                   &Daemon::HandleIncreaseScreenBrightnessMethod);
  ExportDBusMethod(kGetScreenBrightnessPercentMethod,
                   &Daemon::HandleGetScreenBrightnessMethod);
  ExportDBusMethod(kSetScreenBrightnessPercentMethod,
                   &Daemon::HandleSetScreenBrightnessMethod);
  ExportDBusMethod(kDecreaseKeyboardBrightnessMethod,
                   &Daemon::HandleDecreaseKeyboardBrightnessMethod);
  ExportDBusMethod(kIncreaseKeyboardBrightnessMethod,
                   &Daemon::HandleIncreaseKeyboardBrightnessMethod);
  ExportDBusMethod(kGetPowerSupplyPropertiesMethod,
                   &Daemon::HandleGetPowerSupplyPropertiesMethod);
  ExportDBusMethod(kHandleVideoActivityMethod,
                   &Daemon::HandleVideoActivityMethod);
  ExportDBusMethod(kHandleUserActivityMethod,
                   &Daemon::HandleUserActivityMethod);
  ExportDBusMethod(kSetIsProjectingMethod,
                   &Daemon::HandleSetIsProjectingMethod);
  ExportDBusMethod(kSetPolicyMethod,
                   &Daemon::HandleSetPolicyMethod);
  ExportDBusMethod(kHandlePowerButtonAcknowledgmentMethod,
                   &Daemon::HandlePowerButtonAcknowledgment);
  CHECK(powerd_dbus_object_->ExportMethodAndBlock(
      kPowerManagerInterface, kRegisterSuspendDelayMethod,
      base::Bind(&policy::Suspender::RegisterSuspendDelay,
                 base::Unretained(suspender_.get()))));
  CHECK(powerd_dbus_object_->ExportMethodAndBlock(
      kPowerManagerInterface, kUnregisterSuspendDelayMethod,
      base::Bind(&policy::Suspender::UnregisterSuspendDelay,
                 base::Unretained(suspender_.get()))));
  CHECK(powerd_dbus_object_->ExportMethodAndBlock(
      kPowerManagerInterface, kHandleSuspendReadinessMethod,
      base::Bind(&policy::Suspender::HandleSuspendReadiness,
                 base::Unretained(suspender_.get()))));

  // Note that this needs to happen *after* the above methods are exported
  // (http://crbug.com/331431).
  CHECK(bus_->RequestOwnershipAndBlock(kPowerManagerServiceName,
                                       dbus::Bus::REQUIRE_PRIMARY))
      << "Unable to take ownership of " << kPowerManagerServiceName;

  // Listen for NameOwnerChanged signals from the bus itself. Register for all
  // of these signals instead of calling individual proxies'
  // SetNameOwnerChangedCallback() methods so that Suspender can get notified
  // when clients with suspend delays for which Daemon doesn't have proxies
  // disconnect.
  const char kBusServiceName[] = "org.freedesktop.DBus";
  const char kBusServicePath[] = "/org/freedesktop/DBus";
  const char kBusInterface[] = "org.freedesktop.DBus";
  const char kNameOwnerChangedSignal[] = "NameOwnerChanged";
  dbus::ObjectProxy* proxy = bus_->GetObjectProxy(
      kBusServiceName, dbus::ObjectPath(kBusServicePath));
  proxy->ConnectToSignal(kBusInterface, kNameOwnerChangedSignal,
      base::Bind(&Daemon::HandleDBusNameOwnerChanged, base::Unretained(this)),
      base::Bind(&Daemon::HandleDBusSignalConnected, base::Unretained(this)));

  dbus_sender_->Init(powerd_dbus_object_, kPowerManagerInterface);
}

void Daemon::HandleChromeAvailableOrRestarted(bool available) {
  if (!available) {
    LOG(ERROR) << "Failed waiting for Chrome to become available";
    return;
  }
  if (display_backlight_controller_)
    display_backlight_controller_->HandleChromeStart();
}

void Daemon::HandleSessionManagerAvailableOrRestarted(bool available) {
  if (!available) {
    LOG(ERROR) << "Failed waiting for session manager to become available";
    return;
  }

  dbus::MethodCall method_call(
      login_manager::kSessionManagerInterface,
      login_manager::kSessionManagerRetrieveSessionState);
  scoped_ptr<dbus::Response> response(
      session_manager_dbus_proxy_->CallMethodAndBlock(
          &method_call, kSessionManagerDBusTimeoutMs));
  if (!response)
    return;

  std::string state;
  dbus::MessageReader reader(response.get());
  if (!reader.PopString(&state)) {
    LOG(ERROR) << "Unable to read "
               << login_manager::kSessionManagerRetrieveSessionState << " args";
    return;
  }
  OnSessionStateChange(state);
}

void Daemon::HandleCrasAvailableOrRestarted(bool available) {
  if (!available) {
    LOG(ERROR) << "Failed waiting for CRAS to become available";
    return;
  }
  audio_client_->LoadInitialState();
}

void Daemon::HandleUpdateEngineAvailableOrRestarted(bool available) {
  if (!available) {
    LOG(ERROR) << "Failed waiting for update engine to become available";
    return;
  }

  dbus::MethodCall method_call(update_engine::kUpdateEngineInterface,
                               update_engine::kGetStatus);
  scoped_ptr<dbus::Response> response(
      update_engine_dbus_proxy_->CallMethodAndBlock(
          &method_call, kUpdateEngineDBusTimeoutMs));
  if (!response)
    return;

  dbus::MessageReader reader(response.get());
  int64 last_checked_time = 0;
  double progress = 0.0;
  std::string operation;
  if (!reader.PopInt64(&last_checked_time) ||
      !reader.PopDouble(&progress) ||
      !reader.PopString(&operation)) {
    LOG(ERROR) << "Unable to read " << update_engine::kGetStatus << " args";
    return;
  }
  OnUpdateOperation(operation);
}

void Daemon::HandleDBusNameOwnerChanged(dbus::Signal* signal) {
  dbus::MessageReader reader(signal);
  std::string name, old_owner, new_owner;
  if (!reader.PopString(&name) ||
      !reader.PopString(&old_owner) ||
      !reader.PopString(&new_owner)) {
    LOG(ERROR) << "Unable to parse NameOwnerChanged signal";
    return;
  }

  if (name == login_manager::kSessionManagerServiceName && !new_owner.empty()) {
    LOG(INFO) << "D-Bus " << name << " ownership changed to " << new_owner;
    HandleSessionManagerAvailableOrRestarted(true);
  } else if (name == cras::kCrasServiceName && !new_owner.empty()) {
    LOG(INFO) << "D-Bus " << name << " ownership changed to " << new_owner;
    HandleCrasAvailableOrRestarted(true);
  } else if (name == chromeos::kLibCrosServiceName && !new_owner.empty()) {
    LOG(INFO) << "D-Bus " << name << " ownership changed to " << new_owner;
    HandleChromeAvailableOrRestarted(true);
  }
  suspender_->HandleDBusNameOwnerChanged(name, old_owner, new_owner);
}

void Daemon::HandleDBusSignalConnected(const std::string& interface,
                                       const std::string& signal,
                                       bool success) {
  if (!success)
    LOG(ERROR) << "Failed to connect to " << interface << "." << signal;
}

void Daemon::ExportDBusMethod(const std::string& method_name,
                              DBusMethodCallMemberFunction member) {
  DCHECK(powerd_dbus_object_);
  CHECK(powerd_dbus_object_->ExportMethodAndBlock(
      kPowerManagerInterface, method_name,
      base::Bind(&HandleSynchronousDBusMethodCall,
                 base::Bind(member, base::Unretained(this)))));
}

void Daemon::HandleSessionStateChangedSignal(dbus::Signal* signal) {
  dbus::MessageReader reader(signal);
  std::string state;
  if (reader.PopString(&state)) {
    OnSessionStateChange(state);
  } else {
    LOG(ERROR) << "Unable to read "
               << login_manager::kSessionStateChangedSignal << " args";
  }
}

void Daemon::HandleUpdateEngineStatusUpdateSignal(dbus::Signal* signal) {
  dbus::MessageReader reader(signal);
  int64 last_checked_time = 0;
  double progress = 0.0;
  std::string operation;
  if (!reader.PopInt64(&last_checked_time) ||
      !reader.PopDouble(&progress) ||
      !reader.PopString(&operation)) {
    LOG(ERROR) << "Unable to read " << update_engine::kStatusUpdate << " args";
    return;
  }
  OnUpdateOperation(operation);
}

void Daemon::HandleCrasNodesChangedSignal(dbus::Signal* signal) {
  audio_client_->UpdateDevices();
}

void Daemon::HandleCrasActiveOutputNodeChangedSignal(dbus::Signal* signal) {
  audio_client_->UpdateDevices();
}

void Daemon::HandleCrasNumberOfActiveStreamsChanged(dbus::Signal* signal) {
  audio_client_->UpdateNumActiveStreams();
}

scoped_ptr<dbus::Response> Daemon::HandleRequestShutdownMethod(
    dbus::MethodCall* method_call) {
  LOG(INFO) << "Got " << kRequestShutdownMethod << " message";
  ShutDown(SHUTDOWN_MODE_POWER_OFF, SHUTDOWN_REASON_USER_REQUEST);
  return scoped_ptr<dbus::Response>();
}

scoped_ptr<dbus::Response> Daemon::HandleRequestRestartMethod(
    dbus::MethodCall* method_call) {
  LOG(INFO) << "Got " << kRequestRestartMethod << " message";
  ShutDown(SHUTDOWN_MODE_REBOOT, SHUTDOWN_REASON_USER_REQUEST);
  return scoped_ptr<dbus::Response>();
}

scoped_ptr<dbus::Response> Daemon::HandleRequestSuspendMethod(
    dbus::MethodCall* method_call) {
  // Read an optional uint64 argument specifying the wakeup count that is
  // expected.
  dbus::MessageReader reader(method_call);
  uint64 external_wakeup_count = 0;
  const bool got_external_wakeup_count = reader.PopUint64(
      &external_wakeup_count);
  LOG(INFO) << "Got " << kRequestSuspendMethod << " message"
            << (got_external_wakeup_count ?
                base::StringPrintf(" with external wakeup count %" PRIu64,
                                   external_wakeup_count).c_str() : "");
  Suspend(got_external_wakeup_count, external_wakeup_count);
  return scoped_ptr<dbus::Response>();
}

scoped_ptr<dbus::Response> Daemon::HandleDecreaseScreenBrightnessMethod(
    dbus::MethodCall* method_call) {
  if (!display_backlight_controller_)
    return CreateNotSupportedError(method_call, "Backlight uninitialized");

  bool allow_off = false;
  dbus::MessageReader reader(method_call);
  if (!reader.PopBool(&allow_off))
    LOG(ERROR) << "Missing " << kDecreaseScreenBrightnessMethod << " arg";
  bool changed =
      display_backlight_controller_->DecreaseUserBrightness(allow_off);
  double percent = 0.0;
  if (!changed &&
      display_backlight_controller_->GetBrightnessPercent(&percent)) {
    SendBrightnessChangedSignal(
        percent, policy::BacklightController::BRIGHTNESS_CHANGE_USER_INITIATED,
        kBrightnessChangedSignal);
  }
  return scoped_ptr<dbus::Response>();
}

scoped_ptr<dbus::Response> Daemon::HandleIncreaseScreenBrightnessMethod(
    dbus::MethodCall* method_call) {
  if (!display_backlight_controller_)
    return CreateNotSupportedError(method_call, "Backlight uninitialized");

  bool changed = display_backlight_controller_->IncreaseUserBrightness();
  double percent = 0.0;
  if (!changed &&
      display_backlight_controller_->GetBrightnessPercent(&percent)) {
    SendBrightnessChangedSignal(
        percent, policy::BacklightController::BRIGHTNESS_CHANGE_USER_INITIATED,
        kBrightnessChangedSignal);
  }
  return scoped_ptr<dbus::Response>();
}

scoped_ptr<dbus::Response> Daemon::HandleSetScreenBrightnessMethod(
    dbus::MethodCall* method_call) {
  if (!display_backlight_controller_)
    return CreateNotSupportedError(method_call, "Backlight uninitialized");

  double percent = 0.0;
  int dbus_style = 0;
  dbus::MessageReader reader(method_call);
  if (!reader.PopDouble(&percent) || !reader.PopInt32(&dbus_style)) {
    LOG(ERROR) << "Missing " << kSetScreenBrightnessPercentMethod << " args";
    return CreateInvalidArgsError(method_call, "Expected percent and style");
  }

  policy::BacklightController::TransitionStyle style =
      policy::BacklightController::TRANSITION_FAST;
  switch (dbus_style) {
    case kBrightnessTransitionGradual:
      style = policy::BacklightController::TRANSITION_FAST;
      break;
    case kBrightnessTransitionInstant:
      style = policy::BacklightController::TRANSITION_INSTANT;
      break;
    default:
      LOG(ERROR) << "Invalid transition style (" << dbus_style << ")";
  }
  display_backlight_controller_->SetUserBrightnessPercent(percent, style);
  return scoped_ptr<dbus::Response>();
}

scoped_ptr<dbus::Response> Daemon::HandleGetScreenBrightnessMethod(
    dbus::MethodCall* method_call) {
  if (!display_backlight_controller_)
    return CreateNotSupportedError(method_call, "Backlight uninitialized");

  double percent = 0.0;
  if (!display_backlight_controller_->GetBrightnessPercent(&percent)) {
    return scoped_ptr<dbus::Response>(dbus::ErrorResponse::FromMethodCall(
        method_call, DBUS_ERROR_FAILED, "Couldn't fetch brightness"));
  }
  scoped_ptr<dbus::Response> response(
      dbus::Response::FromMethodCall(method_call));
  dbus::MessageWriter writer(response.get());
  writer.AppendDouble(percent);
  return response.Pass();
}

scoped_ptr<dbus::Response> Daemon::HandleDecreaseKeyboardBrightnessMethod(
    dbus::MethodCall* method_call) {
  AdjustKeyboardBrightness(-1);
  return scoped_ptr<dbus::Response>();
}

scoped_ptr<dbus::Response> Daemon::HandleIncreaseKeyboardBrightnessMethod(
    dbus::MethodCall* method_call) {
  AdjustKeyboardBrightness(1);
  return scoped_ptr<dbus::Response>();
}

scoped_ptr<dbus::Response> Daemon::HandleGetPowerSupplyPropertiesMethod(
    dbus::MethodCall* method_call) {
  PowerSupplyProperties protobuf;
  CopyPowerStatusToProtocolBuffer(power_supply_->power_status(), &protobuf);
  scoped_ptr<dbus::Response> response(
      dbus::Response::FromMethodCall(method_call));
  dbus::MessageWriter writer(response.get());
  writer.AppendProtoAsArrayOfBytes(protobuf);
  return response.Pass();
}

scoped_ptr<dbus::Response> Daemon::HandleVideoActivityMethod(
    dbus::MethodCall* method_call) {
  bool is_fullscreen = false;
  dbus::MessageReader reader(method_call);
  if (!reader.PopBool(&is_fullscreen))
    LOG(ERROR) << "Unable to read " << kHandleVideoActivityMethod << " args";

  if (keyboard_backlight_controller_)
    keyboard_backlight_controller_->HandleVideoActivity(is_fullscreen);
  state_controller_->HandleVideoActivity();
  return scoped_ptr<dbus::Response>();
}

scoped_ptr<dbus::Response> Daemon::HandleUserActivityMethod(
    dbus::MethodCall* method_call) {
  int type_int = USER_ACTIVITY_OTHER;
  dbus::MessageReader reader(method_call);
  if (!reader.PopInt32(&type_int))
    LOG(ERROR) << "Unable to read " << kHandleUserActivityMethod << " args";
  UserActivityType type = static_cast<UserActivityType>(type_int);

  suspender_->HandleUserActivity();
  state_controller_->HandleUserActivity();
  if (display_backlight_controller_)
    display_backlight_controller_->HandleUserActivity(type);
  if (keyboard_backlight_controller_)
    keyboard_backlight_controller_->HandleUserActivity(type);
  return scoped_ptr<dbus::Response>();
}

scoped_ptr<dbus::Response> Daemon::HandleSetIsProjectingMethod(
    dbus::MethodCall* method_call) {
  bool is_projecting = false;
  dbus::MessageReader reader(method_call);
  if (!reader.PopBool(&is_projecting)) {
    LOG(ERROR) << "Unable to read " << kSetIsProjectingMethod << " args";
    return CreateInvalidArgsError(method_call, "Expected boolean state");
  }

  DisplayMode mode = is_projecting ? DISPLAY_PRESENTATION : DISPLAY_NORMAL;
  state_controller_->HandleDisplayModeChange(mode);
  if (display_backlight_controller_)
    display_backlight_controller_->HandleDisplayModeChange(mode);
  return scoped_ptr<dbus::Response>();
}

scoped_ptr<dbus::Response> Daemon::HandleSetPolicyMethod(
    dbus::MethodCall* method_call) {
  PowerManagementPolicy policy;
  dbus::MessageReader reader(method_call);
  if (!reader.PopArrayOfBytesAsProto(&policy)) {
    LOG(ERROR) << "Unable to parse " << kSetPolicyMethod << " request";
    return CreateInvalidArgsError(method_call, "Expected protobuf");
  }

  state_controller_->HandlePolicyChange(policy);
  if (display_backlight_controller_)
    display_backlight_controller_->HandlePolicyChange(policy);
  return scoped_ptr<dbus::Response>();
}

scoped_ptr<dbus::Response> Daemon::HandlePowerButtonAcknowledgment(
    dbus::MethodCall* method_call) {
  int64 timestamp_internal = 0;
  dbus::MessageReader reader(method_call);
  if (!reader.PopInt64(&timestamp_internal)) {
    LOG(ERROR) << "Unable to parse " << kHandlePowerButtonAcknowledgmentMethod
               << " request";
    return CreateInvalidArgsError(method_call, "Expected int64 timestamp");
  }
  input_controller_->HandlePowerButtonAcknowledgment(
      base::TimeTicks::FromInternalValue(timestamp_internal));
  return scoped_ptr<dbus::Response>();
}

void Daemon::OnSessionStateChange(const std::string& state_str) {
  SessionState state = (state_str == kSessionStarted) ?
      SESSION_STARTED : SESSION_STOPPED;
  if (state == session_state_)
    return;

  session_state_ = state;
  metrics_collector_->HandleSessionStateChange(state);
  if (state_controller_initialized_)
    state_controller_->HandleSessionStateChange(state);
  if (display_backlight_controller_)
    display_backlight_controller_->HandleSessionStateChange(state);
  if (keyboard_backlight_controller_)
    keyboard_backlight_controller_->HandleSessionStateChange(state);
}

void Daemon::OnUpdateOperation(const std::string& operation) {
  UpdaterState state = UPDATER_IDLE;
  if (operation == update_engine::kUpdateStatusDownloading ||
      operation == update_engine::kUpdateStatusVerifying ||
      operation == update_engine::kUpdateStatusFinalizing) {
    state = UPDATER_UPDATING;
  } else if (operation == update_engine::kUpdateStatusUpdatedNeedReboot) {
    state = UPDATER_UPDATED;
  }
  state_controller_->HandleUpdaterStateChange(state);
}

void Daemon::ShutDown(ShutdownMode mode, ShutdownReason reason) {
  if (shutting_down_) {
    LOG(WARNING) << "Shutdown already initiated";
    return;
  }

  shutting_down_ = true;
  suspender_->HandleShutdown();
  metrics_collector_->HandleShutdown(reason);

  // If we want to display a low-battery alert while shutting down, don't turn
  // the screen off immediately.
  if (reason != SHUTDOWN_REASON_LOW_BATTERY) {
    if (display_backlight_controller_)
      display_backlight_controller_->SetShuttingDown(true);
    if (keyboard_backlight_controller_)
      keyboard_backlight_controller_->SetShuttingDown(true);
  }

  const std::string reason_str = ShutdownReasonToString(reason);
  switch (mode) {
    case SHUTDOWN_MODE_POWER_OFF:
      LOG(INFO) << "Shutting down, reason: " << reason_str;
      RunSetuidHelper("shut_down", "--shutdown_reason=" + reason_str, false);
      break;
    case SHUTDOWN_MODE_REBOOT:
      LOG(INFO) << "Restarting";
      RunSetuidHelper("reboot", "", false);
      break;
  }
}

void Daemon::Suspend(bool use_external_wakeup_count,
                     uint64 external_wakeup_count) {
  if (shutting_down_) {
    LOG(INFO) << "Ignoring request for suspend with outstanding shutdown";
    return;
  }

  if (use_external_wakeup_count)
    suspender_->RequestSuspendWithExternalWakeupCount(external_wakeup_count);
  else
    suspender_->RequestSuspend();
}

void Daemon::SetBacklightsDimmedForInactivity(bool dimmed) {
  if (display_backlight_controller_)
    display_backlight_controller_->SetDimmedForInactivity(dimmed);
  if (keyboard_backlight_controller_)
    keyboard_backlight_controller_->SetDimmedForInactivity(dimmed);
  metrics_collector_->HandleScreenDimmedChange(
      dimmed, state_controller_->last_user_activity_time());
}

void Daemon::SetBacklightsOffForInactivity(bool off) {
  if (display_backlight_controller_)
    display_backlight_controller_->SetOffForInactivity(off);
  if (keyboard_backlight_controller_)
    keyboard_backlight_controller_->SetOffForInactivity(off);
  metrics_collector_->HandleScreenOffChange(
      off, state_controller_->last_user_activity_time());
}

void Daemon::SetBacklightsSuspended(bool suspended) {
  if (display_backlight_controller_)
    display_backlight_controller_->SetSuspended(suspended);
  if (keyboard_backlight_controller_)
    keyboard_backlight_controller_->SetSuspended(suspended);
}

void Daemon::SetBacklightsDocked(bool docked) {
  if (display_backlight_controller_)
    display_backlight_controller_->SetDocked(docked);
  if (keyboard_backlight_controller_)
    keyboard_backlight_controller_->SetDocked(docked);
}

}  // namespace power_manager
