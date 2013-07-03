// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/daemon.h"

#include <inttypes.h>
#include <libudev.h>
#include <stdint.h>
#include <sys/inotify.h>

#include <algorithm>
#include <cmath>
#include <set>
#include <vector>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "chromeos/dbus/dbus.h"
#include "chromeos/dbus/service_constants.h"
#include "power_manager/common/dbus_sender.h"
#include "power_manager/common/power_constants.h"
#include "power_manager/common/prefs.h"
#include "power_manager/common/util.h"
#include "power_manager/common/util_dbus.h"
#include "power_manager/policy.pb.h"
#include "power_manager/power_supply_properties.pb.h"
#include "power_manager/powerd/metrics_constants.h"
#include "power_manager/powerd/metrics_reporter.h"
#include "power_manager/powerd/policy/backlight_controller.h"
#include "power_manager/powerd/policy/state_controller.h"
#include "power_manager/powerd/system/audio_client.h"
#include "power_manager/powerd/system/input.h"

namespace power_manager {

namespace {

// Path for storing FileTagger files.
const char kTaggedFilePath[] = "/var/lib/power_manager";

// Path to power supply info.
const char kPowerStatusPath[] = "/sys/class/power_supply";

// Power supply subsystem for udev events.
const char kPowerSupplyUdevSubsystem[] = "power_supply";

// Strings for states that powerd cares about from the session manager's
// SessionStateChanged signal.
const char kSessionStarted[] = "started";
const char kSessionStopped[] = "stopped";

// Path containing the number of wakeup events.
const char kWakeupCountPath[] = "/sys/power/wakeup_count";

// Copies fields from |status| into |proto|.
void CopyPowerStatusToProtocolBuffer(const system::PowerStatus& status,
                                     PowerSupplyProperties* proto) {
  DCHECK(proto);
  proto->Clear();
  proto->set_external_power(status.external_power);
  proto->set_battery_state(status.battery_state);
  proto->set_battery_percent(status.display_battery_percentage);
  proto->set_battery_time_to_empty_sec(status.averaged_battery_time_to_empty);
  proto->set_battery_time_to_full_sec(status.averaged_battery_time_to_full);
  proto->set_is_calculating_battery_time(status.is_calculating_battery_time);
}

// Returns a string describing the battery status from |status|.
std::string GetPowerStatusBatteryDebugString(
    const system::PowerStatus& status) {
  if (!status.battery_is_present)
    return std::string();

  std::string output;
  switch (status.external_power) {
    case PowerSupplyProperties_ExternalPower_AC:
    case PowerSupplyProperties_ExternalPower_USB: {
      const char* type = (status.external_power ==
          PowerSupplyProperties_ExternalPower_AC) ? "AC" : "USB";
      output = StringPrintf("On %s (%s", type, status.line_power_type.c_str());
      if (status.line_power_current || status.line_power_voltage) {
        output += StringPrintf(", %.3fA at %.1fV",
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
  output += StringPrintf("%ld%%", rounded_actual);
  if (rounded_actual != rounded_display)
    output += StringPrintf(" (displayed as %ld%%)", rounded_display);
  output += StringPrintf(", %.3f/%.3fAh at %.3fA", status.battery_charge,
      status.battery_charge_full, status.battery_current);

  // TODO(derat): Change this to instead switch on |status.battery_state|
  // and display time-to-full for CHARGING and time-to-empty for DISCHARGING
  // after PowerSupply has been updated.
  switch (status.external_power) {
    case PowerSupplyProperties_ExternalPower_AC:
    case PowerSupplyProperties_ExternalPower_USB:
      if (status.battery_state == PowerSupplyProperties_BatteryState_FULL) {
        output += ", full";
      } else {
        output += ", " + util::TimeDeltaToString(
            base::TimeDelta::FromSeconds(status.battery_time_to_full)) +
            " until full";
        if (status.is_calculating_battery_time)
          output += " (calculating)";
      }
      break;
    case PowerSupplyProperties_ExternalPower_DISCONNECTED:
      output += ", " + util::TimeDeltaToString(
          base::TimeDelta::FromSeconds(status.battery_time_to_empty)) +
          " until empty";
      if (status.is_calculating_battery_time)
        output += " (calculating)";
      break;
  }

  return output;
}

}  // namespace

// Performs actions requested by |state_controller_|.  The reason that
// this is a nested class of Daemon rather than just being implented as
// part of Daemon is to avoid method naming conflicts.
class Daemon::StateControllerDelegate
    : public policy::StateController::Delegate {
 public:
  explicit StateControllerDelegate(Daemon* daemon) : daemon_(daemon) {}

  ~StateControllerDelegate() {
    daemon_ = NULL;
  }

  // Overridden from policy::StateController::Delegate:
  virtual bool IsUsbInputDeviceConnected() OVERRIDE {
    return daemon_->input_->IsUSBInputDeviceConnected();
  }

  virtual bool IsOobeCompleted() OVERRIDE {
    return util::OOBECompleted();
  }

  virtual bool ShouldAvoidSuspendForHeadphoneJack() OVERRIDE {
#ifdef STAY_AWAKE_PLUGGED_DEVICE
    return daemon_->audio_client_->IsHeadphoneJackConnected();
#else
    return false;
#endif
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
    util::CallSessionManagerMethod(
        login_manager::kSessionManagerLockScreen, NULL);
  }

  virtual void Suspend() OVERRIDE {
    daemon_->Suspend();
  }

  virtual void StopSession() OVERRIDE {
    // This session manager method takes a string argument, although it
    // doesn't currently do anything with it.
    util::CallSessionManagerMethod(
        login_manager::kSessionManagerStopSession, "");
  }

  virtual void ShutDown() OVERRIDE {
    // TODO(derat): Maybe pass the shutdown reason (idle vs. lid-closed)
    // and pass it here.  This isn't necessary at the moment, since nothing
    // special is done for any reason besides |kShutdownReasonLowBattery|.
    daemon_->ShutDown(SHUTDOWN_POWER_OFF, kShutdownReasonUnknown);
  }

  virtual void UpdatePanelForDockedMode(bool docked) OVERRIDE {
    daemon_->SetBacklightsDocked(docked);
  }

  virtual void EmitIdleNotification(base::TimeDelta delay) OVERRIDE {
    daemon_->IdleEventNotify(delay.InMilliseconds());
  }

  virtual void EmitIdleActionImminent() OVERRIDE {
    daemon_->dbus_sender_->EmitBareSignal(kIdleActionImminentSignal);
  }

  virtual void EmitIdleActionDeferred() OVERRIDE {
    daemon_->dbus_sender_->EmitBareSignal(kIdleActionDeferredSignal);
  }

  virtual void ReportUserActivityMetrics() OVERRIDE {
    daemon_->metrics_reporter_->GenerateUserActivityMetrics();
  }

 private:
  Daemon* daemon_;  // not owned

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
    if (file_util::ReadFileToString(path, &buf)) {
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
    daemon_->PrepareForSuspendAnnouncement();
  }

  virtual void HandleCanceledSuspendAnnouncement() OVERRIDE {
    daemon_->HandleCanceledSuspendAnnouncement();
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
      args += StringPrintf(" --suspend_wakeup_count_valid"
                           " --suspend_wakeup_count %" PRIu64, wakeup_count);
    }
    if (duration != base::TimeDelta()) {
      args += StringPrintf(" --suspend_duration %" PRId64,
                           duration.InSeconds());
    }

    // These exit codes are defined in scripts/powerd_suspend.
    switch (util::RunSetuidHelper("suspend", args, true)) {
      case 0:
        return SUSPEND_SUCCESSFUL;
      case 1:
        return SUSPEND_FAILED;
      case 2:
        return SUSPEND_CANCELED;
      default:
        LOG(ERROR) << "Treating unexpected exit code as suspend failure";
        return SUSPEND_FAILED;
    }
  }

  virtual void HandleResume(bool suspend_was_successful,
                            int num_suspend_retries,
                            int max_suspend_retries) OVERRIDE {
    SendPowerStateChangedSignal("on");
    daemon_->HandleResume(suspend_was_successful,
                          num_suspend_retries,
                          max_suspend_retries);
  }

  virtual void ShutDownForFailedSuspend() OVERRIDE {
    daemon_->ShutDown(SHUTDOWN_POWER_OFF, kShutdownReasonSuspendFailed);
  }

  virtual void ShutDownForDarkResume() OVERRIDE {
    daemon_->ShutDown(SHUTDOWN_POWER_OFF, kShutdownReasonDarkResume);
  }

 private:
  void SendPowerStateChangedSignal(const std::string& power_state) {
    chromeos::dbus::Proxy proxy(chromeos::dbus::GetSystemBusConnection(),
                                kPowerManagerServicePath,
                                kPowerManagerInterface);
    const char* state = power_state.c_str();
    DBusMessage* signal = dbus_message_new_signal(kPowerManagerServicePath,
                                                  kPowerManagerInterface,
                                                  kPowerStateChanged);
    CHECK(signal);
    dbus_message_append_args(signal,
                             DBUS_TYPE_STRING, &state,
                             DBUS_TYPE_INVALID);
    dbus_g_proxy_send(proxy.gproxy(), signal, NULL);
    dbus_message_unref(signal);
  }

  Daemon* daemon_;  // not owned

  DISALLOW_COPY_AND_ASSIGN(SuspenderDelegate);
};

Daemon::Daemon(PrefsInterface* prefs,
               MetricsLibraryInterface* metrics_lib,
               policy::BacklightController* backlight_controller,
               policy::KeyboardBacklightController* keyboard_controller,
               const base::FilePath& run_dir)
    : state_controller_delegate_(new StateControllerDelegate(this)),
      backlight_controller_(backlight_controller),
      prefs_(prefs),
      keyboard_controller_(keyboard_controller),
      metrics_reporter_(
          new MetricsReporter(
              prefs, metrics_lib, backlight_controller, keyboard_controller)),
      dbus_sender_(
          new DBusSender(kPowerManagerServicePath, kPowerManagerInterface)),
      input_(new system::Input),
      state_controller_(new policy::StateController(
          state_controller_delegate_.get(), prefs_)),
      input_controller_(new policy::InputController(
          input_.get(), this, backlight_controller, state_controller_.get(),
          dbus_sender_.get(), run_dir)),
      audio_client_(new system::AudioClient),
      peripheral_battery_watcher_(new system::PeripheralBatteryWatcher(
          dbus_sender_.get())),
      shutting_down_(false),
      low_battery_(false),
      plugged_state_(PLUGGED_STATE_UNKNOWN),
      file_tagger_(base::FilePath(kTaggedFilePath)),
      power_supply_(
          new system::PowerSupply(base::FilePath(kPowerStatusPath), prefs)),
      dark_resume_policy_(
          new policy::DarkResumePolicy(power_supply_.get(), prefs)),
      suspender_delegate_(new SuspenderDelegate(this)),
      suspender_(suspender_delegate_.get(), dbus_sender_.get(),
                 dark_resume_policy_.get()),
      run_dir_(run_dir),
      session_state_(SESSION_STOPPED),
      udev_(NULL),
      state_controller_initialized_(false) {
  power_supply_->AddObserver(this);
  if (backlight_controller_)
    backlight_controller_->AddObserver(this);
  audio_client_->AddObserver(this);
}

Daemon::~Daemon() {
  audio_client_->RemoveObserver(this);
  if (backlight_controller_)
    backlight_controller_->RemoveObserver(this);
  power_supply_->RemoveObserver(this);

  if (udev_)
    udev_unref(udev_);
}

void Daemon::Init() {
  metrics_reporter_->Init();

  RegisterUdevEventHandler();
  RegisterDBusMessageHandler();

  file_tagger_.Init();

  std::string session_state;
  if (util::GetSessionState(&session_state))
    OnSessionStateChange(session_state);

  power_supply_->Init();
  power_supply_->RefreshImmediately();
  OnPowerStatusUpdate();

  dark_resume_policy_->Init();
  suspender_.Init(prefs_);

  std::string wakeup_inputs_str;
  std::vector<std::string> wakeup_inputs;
  if (prefs_->GetString(kWakeupInputPref, &wakeup_inputs_str))
    base::SplitString(wakeup_inputs_str, '\n', &wakeup_inputs);
  bool use_lid = true;
  prefs_->GetBool(kUseLidPref, &use_lid);
  CHECK(input_->Init(wakeup_inputs, use_lid));

  input_controller_->Init(prefs_);

  std::string headphone_device;
#ifdef STAY_AWAKE_PLUGGED_DEVICE
  headphone_device = STAY_AWAKE_PLUGGED_DEVICE;
#endif
  audio_client_->Init(headphone_device);

  PowerSource power_source =
      plugged_state_ == PLUGGED_STATE_DISCONNECTED ? POWER_BATTERY : POWER_AC;
  state_controller_->Init(power_source, input_->QueryLidState(),
                          session_state_, DISPLAY_NORMAL);
  state_controller_initialized_ = true;

  peripheral_battery_watcher_->Init();

  // TODO(crosbug.com/31927): Send a signal to announce that powerd has started.
  // This is necessary for receiving external display projection status from
  // Chrome, for instance.
}

void Daemon::Run() {
  GMainLoop* loop = g_main_loop_new(NULL, false);
  g_main_loop_run(loop);
}

void Daemon::SetPlugged(bool plugged) {
  if (plugged == plugged_state_)
    return;

  // TODO(derat): Move all of this logic into MetricsReporter and make
  // Daemon just notify it about session state changes, power source
  // changes, and so on.
  if (plugged)
    metrics_reporter_->GenerateNumOfSessionsPerChargeMetric();
  else if (session_state_ == SESSION_STARTED)
    metrics_reporter_->IncrementNumOfSessionsPerChargeMetric();

  // If we are moving from PLUGGED_STATE_UNKNOWN then we don't know how
  // long the device has been on AC for and thus our metric would not tell
  // us anything about the battery state when the user decided to charge.
  if (plugged_state_ != PLUGGED_STATE_UNKNOWN) {
    metrics_reporter_->GenerateBatteryInfoWhenChargeStartsMetric(
        power_supply_->power_status());
  }

  plugged_state_ = plugged ? PLUGGED_STATE_CONNECTED :
      PLUGGED_STATE_DISCONNECTED;

  PowerSource power_source = plugged ? POWER_AC : POWER_BATTERY;
  metrics_reporter_->HandlePowerSourceChange(power_source);
  if (backlight_controller_)
    backlight_controller_->HandlePowerSourceChange(power_source);
  if (keyboard_controller_)
    keyboard_controller_->HandlePowerSourceChange(power_source);
  if (state_controller_initialized_)
    state_controller_->HandlePowerSourceChange(power_source);
}

void Daemon::IdleEventNotify(int64 threshold) {
  dbus_int64_t threshold_int =
      static_cast<dbus_int64_t>(threshold);

  chromeos::dbus::Proxy proxy(chromeos::dbus::GetSystemBusConnection(),
                              kPowerManagerServicePath,
                              kPowerManagerInterface);
  DBusMessage* signal = dbus_message_new_signal(
      kPowerManagerServicePath,
      kPowerManagerInterface,
      kIdleNotifySignal);
  CHECK(signal);
  dbus_message_append_args(signal,
                           DBUS_TYPE_INT64, &threshold_int,
                           DBUS_TYPE_INVALID);
  dbus_g_proxy_send(proxy.gproxy(), signal, NULL);
  dbus_message_unref(signal);
}

void Daemon::AdjustKeyboardBrightness(int direction) {
  if (!keyboard_controller_)
    return;

  if (direction > 0)
    keyboard_controller_->IncreaseUserBrightness();
  else if (direction < 0)
    keyboard_controller_->DecreaseUserBrightness(true /* allow_off */);
}

void Daemon::SendBrightnessChangedSignal(
    double brightness_percent,
    policy::BacklightController::BrightnessChangeCause cause,
    const std::string& signal_name) {
  dbus_int32_t brightness_percent_int =
      static_cast<dbus_int32_t>(round(brightness_percent));

  dbus_bool_t user_initiated = FALSE;
  switch (cause) {
    case policy::BacklightController::BRIGHTNESS_CHANGE_AUTOMATED:
      user_initiated = FALSE;
      break;
    case policy::BacklightController::BRIGHTNESS_CHANGE_USER_INITIATED:
      user_initiated = TRUE;
      break;
    default:
      NOTREACHED() << "Unhandled brightness change cause " << cause;
  }

  chromeos::dbus::Proxy proxy(chromeos::dbus::GetSystemBusConnection(),
                              kPowerManagerServicePath,
                              kPowerManagerInterface);
  DBusMessage* signal = dbus_message_new_signal(kPowerManagerServicePath,
                                                kPowerManagerInterface,
                                                signal_name.c_str());
  CHECK(signal);
  dbus_message_append_args(signal,
                           DBUS_TYPE_INT32, &brightness_percent_int,
                           DBUS_TYPE_BOOLEAN, &user_initiated,
                           DBUS_TYPE_INVALID);
  dbus_g_proxy_send(proxy.gproxy(), signal, NULL);
  dbus_message_unref(signal);
}

void Daemon::OnBrightnessChanged(
    double brightness_percent,
    policy::BacklightController::BrightnessChangeCause cause,
    policy::BacklightController* source) {
  if (source == backlight_controller_ && backlight_controller_) {
    SendBrightnessChangedSignal(brightness_percent, cause,
                                kBrightnessChangedSignal);
  } else if (source == keyboard_controller_ && keyboard_controller_) {
    SendBrightnessChangedSignal(brightness_percent, cause,
                                kKeyboardBrightnessChangedSignal);
  } else {
    NOTREACHED() << "Received a brightness change callback from an unknown "
                 << "backlight controller";
  }
}

void Daemon::PrepareForSuspendAnnouncement() {
  // When going to suspend, notify the backlight controller so it can turn
  // the backlight off and tell the kernel to resume the current level
  // after resuming.  This must occur before Chrome is told that the system
  // is going to suspend (Chrome turns the display back on while leaving
  // the backlight off).
  SetBacklightsSuspended(true);
}

void Daemon::HandleCanceledSuspendAnnouncement() {
  // Undo the earlier BACKLIGHT_SUSPENDED call.
  SetBacklightsSuspended(false);
}

void Daemon::PrepareForSuspend() {
#ifdef SUSPEND_LOCK_VT
  // Do not let suspend change the console terminal.
  util::RunSetuidHelper("lock_vt", "", true);
#endif

  power_supply_->SetSuspended(true);
  file_tagger_.HandleSuspendEvent();
  audio_client_->MuteSystem();
  metrics_reporter_->PrepareForSuspend(power_supply_->power_status(),
                                       base::Time::Now());
}

void Daemon::HandleResume(bool suspend_was_successful,
                          int num_suspend_retries,
                          int max_suspend_retries) {
  SetBacklightsSuspended(false);
  audio_client_->RestoreMutedState();

#ifdef SUSPEND_LOCK_VT
  // Allow virtual terminal switching again.
  util::RunSetuidHelper("unlock_vt", "", true);
#endif

  file_tagger_.HandleResumeEvent();
  power_supply_->SetSuspended(false);
  state_controller_->HandleResume();
  if (suspend_was_successful) {
    metrics_reporter_->GenerateRetrySuspendMetric(
        num_suspend_retries, max_suspend_retries);
    metrics_reporter_->HandleResume();
  }
}

void Daemon::HandleLidClosed() {
  if (state_controller_initialized_)
    state_controller_->HandleLidStateChange(LID_CLOSED);
}

void Daemon::HandleLidOpened() {
  suspender_.HandleLidOpened();
  if (state_controller_initialized_)
    state_controller_->HandleLidStateChange(LID_OPEN);
}

void Daemon::SendPowerButtonMetric(bool down, base::TimeTicks timestamp) {
  metrics_reporter_->GeneratePowerButtonMetric(down, timestamp);
}

void Daemon::OnAudioActivity(base::TimeTicks last_activity_time) {
  state_controller_->HandleAudioActivity();
}

void Daemon::OnPowerStatusUpdate() {
  const system::PowerStatus& status = power_supply_->power_status();
  if (status.battery_is_present)
    LOG(INFO) << GetPowerStatusBatteryDebugString(status);

  SetPlugged(status.line_power_on);
  metrics_reporter_->GenerateMetricsOnPowerEvent(status);

  if (status.battery_is_present) {
    if (status.battery_below_shutdown_threshold) {
      if (!low_battery_) {
        low_battery_ = true;
        file_tagger_.HandleLowBatteryEvent();
        LOG(INFO) << "Shutting down due to low battery";
        ShutDown(SHUTDOWN_POWER_OFF, kShutdownReasonLowBattery);
      }
    } else {
      low_battery_ = false;
      file_tagger_.HandleSafeBatteryEvent();
    }
  }

  PowerSupplyProperties protobuf;
  CopyPowerStatusToProtocolBuffer(status, &protobuf);
  dbus_sender_->EmitSignalWithProtocolBuffer(kPowerSupplyPollSignal, protobuf);
}

gboolean Daemon::UdevEventHandler(GIOChannel* /* source */,
                                  GIOCondition /* condition */,
                                  gpointer data) {
  Daemon* daemon = static_cast<Daemon*>(data);

  struct udev_device* dev = udev_monitor_receive_device(daemon->udev_monitor_);
  if (dev) {
    LOG(INFO) << "Event on (" << udev_device_get_subsystem(dev) << ") Action "
              << udev_device_get_action(dev);
    CHECK(std::string(udev_device_get_subsystem(dev)) ==
          kPowerSupplyUdevSubsystem);
    udev_device_unref(dev);
    daemon->power_supply_->HandleUdevEvent();
  } else {
    LOG(ERROR) << "Can't get receive_device()";
    return FALSE;
  }
  return TRUE;
}

void Daemon::RegisterUdevEventHandler() {
  // Create the udev object.
  udev_ = udev_new();
  if (!udev_)
    LOG(ERROR) << "Can't create udev object.";

  // Create the udev monitor structure.
  udev_monitor_ = udev_monitor_new_from_netlink(udev_, "udev");
  if (!udev_monitor_) {
    LOG(ERROR) << "Can't create udev monitor.";
    udev_unref(udev_);
  }
  udev_monitor_filter_add_match_subsystem_devtype(udev_monitor_,
                                                  kPowerSupplyUdevSubsystem,
                                                  NULL);
  udev_monitor_enable_receiving(udev_monitor_);

  int fd = udev_monitor_get_fd(udev_monitor_);

  GIOChannel* channel = g_io_channel_unix_new(fd);
  g_io_add_watch(channel, G_IO_IN, &(Daemon::UdevEventHandler), this);

  LOG(INFO) << "Udev controller waiting for events on subsystem "
            << kPowerSupplyUdevSubsystem;
}

void Daemon::RegisterDBusMessageHandler() {
  util::RequestDBusServiceName(kPowerManagerServiceName);

  dbus_handler_.SetNameOwnerChangedHandler(
      base::Bind(&Daemon::HandleDBusNameOwnerChanged, base::Unretained(this)));

  dbus_handler_.AddSignalHandler(
      login_manager::kSessionManagerInterface,
      login_manager::kSessionStateChangedSignal,
      base::Bind(&Daemon::HandleSessionStateChangedSignal,
                 base::Unretained(this)));
  dbus_handler_.AddSignalHandler(
      update_engine::kUpdateEngineInterface,
      update_engine::kStatusUpdate,
      base::Bind(&Daemon::HandleUpdateEngineStatusUpdateSignal,
                 base::Unretained(this)));

  dbus_handler_.AddMethodHandler(
      kPowerManagerInterface,
      kRequestShutdownMethod,
      base::Bind(&Daemon::HandleRequestShutdownMethod, base::Unretained(this)));
  dbus_handler_.AddMethodHandler(
      kPowerManagerInterface,
      kRequestRestartMethod,
      base::Bind(&Daemon::HandleRequestRestartMethod, base::Unretained(this)));
  dbus_handler_.AddMethodHandler(
      kPowerManagerInterface,
      kRequestSuspendMethod,
      base::Bind(&Daemon::HandleRequestSuspendMethod, base::Unretained(this)));
  dbus_handler_.AddMethodHandler(
      kPowerManagerInterface,
      kDecreaseScreenBrightness,
      base::Bind(&Daemon::HandleDecreaseScreenBrightnessMethod,
                 base::Unretained(this)));
  dbus_handler_.AddMethodHandler(
      kPowerManagerInterface,
      kIncreaseScreenBrightness,
      base::Bind(&Daemon::HandleIncreaseScreenBrightnessMethod,
                 base::Unretained(this)));
  dbus_handler_.AddMethodHandler(
      kPowerManagerInterface,
      kGetScreenBrightnessPercent,
      base::Bind(&Daemon::HandleGetScreenBrightnessMethod,
                 base::Unretained(this)));
  dbus_handler_.AddMethodHandler(
      kPowerManagerInterface,
      kSetScreenBrightnessPercent,
      base::Bind(&Daemon::HandleSetScreenBrightnessMethod,
                 base::Unretained(this)));
  dbus_handler_.AddMethodHandler(
      kPowerManagerInterface,
      kDecreaseKeyboardBrightness,
      base::Bind(&Daemon::HandleDecreaseKeyboardBrightnessMethod,
                 base::Unretained(this)));
  dbus_handler_.AddMethodHandler(
      kPowerManagerInterface,
      kIncreaseKeyboardBrightness,
      base::Bind(&Daemon::HandleIncreaseKeyboardBrightnessMethod,
                 base::Unretained(this)));
  dbus_handler_.AddMethodHandler(
      kPowerManagerInterface,
      kRequestIdleNotification,
      base::Bind(&Daemon::HandleRequestIdleNotificationMethod,
                 base::Unretained(this)));
  dbus_handler_.AddMethodHandler(
      kPowerManagerInterface,
      kGetPowerSupplyPropertiesMethod,
      base::Bind(&Daemon::HandleGetPowerSupplyPropertiesMethod,
                 base::Unretained(this)));
  dbus_handler_.AddMethodHandler(
      kPowerManagerInterface,
      kHandleVideoActivityMethod,
      base::Bind(&Daemon::HandleVideoActivityMethod, base::Unretained(this)));
  dbus_handler_.AddMethodHandler(
      kPowerManagerInterface,
      kHandleUserActivityMethod,
      base::Bind(&Daemon::HandleUserActivityMethod, base::Unretained(this)));
  dbus_handler_.AddMethodHandler(
      kPowerManagerInterface,
      kSetIsProjectingMethod,
      base::Bind(&Daemon::HandleSetIsProjectingMethod, base::Unretained(this)));
  dbus_handler_.AddMethodHandler(
      kPowerManagerInterface,
      kSetPolicyMethod,
      base::Bind(&Daemon::HandleSetPolicyMethod, base::Unretained(this)));
  dbus_handler_.AddMethodHandler(
      kPowerManagerInterface,
      kRegisterSuspendDelayMethod,
      base::Bind(&policy::Suspender::RegisterSuspendDelay,
                 base::Unretained(&suspender_)));
  dbus_handler_.AddMethodHandler(
      kPowerManagerInterface,
      kUnregisterSuspendDelayMethod,
      base::Bind(&policy::Suspender::UnregisterSuspendDelay,
                 base::Unretained(&suspender_)));
  dbus_handler_.AddMethodHandler(
      kPowerManagerInterface,
      kHandleSuspendReadinessMethod,
      base::Bind(&policy::Suspender::HandleSuspendReadiness,
                 base::Unretained(&suspender_)));

  dbus_handler_.Start();
}

void Daemon::HandleDBusNameOwnerChanged(const std::string& name,
                                        const std::string& old_owner,
                                        const std::string& new_owner) {
  suspender_.HandleDBusNameOwnerChanged(name, old_owner, new_owner);
}

bool Daemon::HandleSessionStateChangedSignal(DBusMessage* message) {
  DBusError error;
  dbus_error_init(&error);
  const char* state = NULL;
  if (dbus_message_get_args(message, &error,
                            DBUS_TYPE_STRING, &state,
                            DBUS_TYPE_INVALID)) {
    OnSessionStateChange(state ? state : "");
  } else {
    LOG(WARNING) << "Unable to read "
                 << login_manager::kSessionStateChangedSignal << " args";
    dbus_error_free(&error);
  }
  return false;
}

bool Daemon::HandleUpdateEngineStatusUpdateSignal(DBusMessage* message) {
  DBusError error;
  dbus_error_init(&error);

  dbus_int64_t last_checked_time = 0;
  double progress = 0.0;
  const char* current_operation = NULL;
  const char* new_version = NULL;
  dbus_int64_t new_size = 0;

  if (!dbus_message_get_args(message, &error,
                             DBUS_TYPE_INT64, &last_checked_time,
                             DBUS_TYPE_DOUBLE, &progress,
                             DBUS_TYPE_STRING, &current_operation,
                             DBUS_TYPE_STRING, &new_version,
                             DBUS_TYPE_INT64, &new_size,
                             DBUS_TYPE_INVALID)) {
    LOG(WARNING) << "Unable to read args from " << update_engine::kStatusUpdate
                 << " signal";
    dbus_error_free(&error);
    return false;
  }

  // TODO: Use shared constants instead: http://crosbug.com/39706
  UpdaterState state = UPDATER_IDLE;
  std::string operation = current_operation;
  if (operation == "UPDATE_STATUS_DOWNLOADING" ||
      operation == "UPDATE_STATUS_VERIFYING" ||
      operation == "UPDATE_STATUS_FINALIZING") {
    state = UPDATER_UPDATING;
  } else if (operation == "UPDATE_STATUS_UPDATED_NEED_REBOOT") {
    state = UPDATER_UPDATED;
  }
  state_controller_->HandleUpdaterStateChange(state);

  return false;
}

DBusMessage* Daemon::HandleRequestShutdownMethod(DBusMessage* message) {
  ShutDown(SHUTDOWN_POWER_OFF, kShutdownReasonUserRequest);
  return NULL;
}

DBusMessage* Daemon::HandleRequestRestartMethod(DBusMessage* message) {
  ShutDown(SHUTDOWN_REBOOT, kShutdownReasonUserRequest);
  return NULL;
}

DBusMessage* Daemon::HandleRequestSuspendMethod(DBusMessage* message) {
  Suspend();
  return NULL;
}

DBusMessage* Daemon::HandleDecreaseScreenBrightnessMethod(
    DBusMessage* message) {
  if (!backlight_controller_)
    return util::CreateDBusErrorReply(message, "Backlight uninitialized");

  dbus_bool_t allow_off = false;
  DBusError error;
  dbus_error_init(&error);
  if (dbus_message_get_args(message, &error,
                            DBUS_TYPE_BOOLEAN, &allow_off,
                            DBUS_TYPE_INVALID) == FALSE) {
    LOG(WARNING) << "Unable to read " << kDecreaseScreenBrightness << " args";
    dbus_error_free(&error);
  }
  bool changed = backlight_controller_->DecreaseUserBrightness(allow_off);
  double percent = 0.0;
  if (!changed && backlight_controller_->GetBrightnessPercent(&percent)) {
    SendBrightnessChangedSignal(
        percent, policy::BacklightController::BRIGHTNESS_CHANGE_USER_INITIATED,
        kBrightnessChangedSignal);
  }
  return NULL;
}

DBusMessage* Daemon::HandleIncreaseScreenBrightnessMethod(
    DBusMessage* message) {
  if (!backlight_controller_)
    return util::CreateDBusErrorReply(message, "Backlight uninitialized");

  bool changed = backlight_controller_->IncreaseUserBrightness();
  double percent = 0.0;
  if (!changed && backlight_controller_->GetBrightnessPercent(&percent)) {
    SendBrightnessChangedSignal(
        percent, policy::BacklightController::BRIGHTNESS_CHANGE_USER_INITIATED,
        kBrightnessChangedSignal);
  }
  return NULL;
}

DBusMessage* Daemon::HandleSetScreenBrightnessMethod(DBusMessage* message) {
  double percent;
  int dbus_style;
  DBusError error;
  dbus_error_init(&error);
  if (!dbus_message_get_args(message, &error,
                             DBUS_TYPE_DOUBLE, &percent,
                             DBUS_TYPE_INT32, &dbus_style,
                             DBUS_TYPE_INVALID)) {
    LOG(WARNING) << kSetScreenBrightnessPercent
                << ": Error reading args: " << error.message;
    dbus_error_free(&error);
    return util::CreateDBusInvalidArgsErrorReply(message);
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
      LOG(WARNING) << "Invalid transition style passed ( " << dbus_style
                  << " ).  Using default fast transition";
  }
  backlight_controller_->SetUserBrightnessPercent(percent, style);
  return NULL;
}

DBusMessage* Daemon::HandleGetScreenBrightnessMethod(DBusMessage* message) {
  if (!backlight_controller_)
    return util::CreateDBusErrorReply(message, "Backlight uninitialized");

  double percent = 0.0;
  if (!backlight_controller_->GetBrightnessPercent(&percent)) {
    return util::CreateDBusErrorReply(
        message, "Could not fetch Screen Brightness");
  }
  DBusMessage* reply = util::CreateEmptyDBusReply(message);
  CHECK(reply);
  dbus_message_append_args(reply,
                           DBUS_TYPE_DOUBLE, &percent,
                           DBUS_TYPE_INVALID);
  return reply;
}

DBusMessage* Daemon::HandleDecreaseKeyboardBrightnessMethod(
    DBusMessage* message) {
  AdjustKeyboardBrightness(-1);
  return NULL;
}

DBusMessage* Daemon::HandleIncreaseKeyboardBrightnessMethod(
    DBusMessage* message) {
  AdjustKeyboardBrightness(1);
  return NULL;
}

DBusMessage* Daemon::HandleRequestIdleNotificationMethod(DBusMessage* message) {
  DBusError error;
  dbus_error_init(&error);
  int64 threshold = 0;
  if (dbus_message_get_args(message, &error,
                            DBUS_TYPE_INT64, &threshold,
                            DBUS_TYPE_INVALID)) {
    state_controller_->AddIdleNotification(
        base::TimeDelta::FromMilliseconds(threshold));
  } else {
    LOG(WARNING) << "Unable to read " << kRequestIdleNotification << " args";
    dbus_error_free(&error);
  }
  return NULL;
}

DBusMessage* Daemon::HandleGetPowerSupplyPropertiesMethod(
    DBusMessage* message) {
  PowerSupplyProperties protobuf;
  CopyPowerStatusToProtocolBuffer(power_supply_->power_status(), &protobuf);
  return util::CreateDBusProtocolBufferReply(message, protobuf);
}

DBusMessage* Daemon::HandleVideoActivityMethod(DBusMessage* message) {
  DBusError error;
  dbus_error_init(&error);
  gboolean is_fullscreen = false;
  if (!dbus_message_get_args(message, &error,
                             DBUS_TYPE_BOOLEAN, &is_fullscreen,
                             DBUS_TYPE_INVALID)) {
    LOG(WARNING) << "Unable to read " << kHandleVideoActivityMethod << "args";
    dbus_error_free(&error);
  }

  if (keyboard_controller_)
    keyboard_controller_->HandleVideoActivity(is_fullscreen);
  state_controller_->HandleVideoActivity();
  return NULL;
}

DBusMessage* Daemon::HandleUserActivityMethod(DBusMessage* message) {
  suspender_.HandleUserActivity();
  state_controller_->HandleUserActivity();
  if (backlight_controller_)
    backlight_controller_->HandleUserActivity();
  return NULL;
}

DBusMessage* Daemon::HandleSetIsProjectingMethod(DBusMessage* message) {
  DBusMessage* reply = NULL;
  DBusError error;
  dbus_error_init(&error);
  bool is_projecting = false;
  dbus_bool_t args_ok =
      dbus_message_get_args(message, &error,
                            DBUS_TYPE_BOOLEAN, &is_projecting,
                            DBUS_TYPE_INVALID);
  if (args_ok) {
    DisplayMode mode = is_projecting ? DISPLAY_PRESENTATION : DISPLAY_NORMAL;
    state_controller_->HandleDisplayModeChange(mode);
    if (backlight_controller_)
      backlight_controller_->HandleDisplayModeChange(mode);
  } else {
    // The message was malformed so log this and return an error.
    LOG(WARNING) << kSetIsProjectingMethod << ": Error reading args: "
                 << error.message;
    reply = util::CreateDBusInvalidArgsErrorReply(message);
  }
  return reply;
}

DBusMessage* Daemon::HandleSetPolicyMethod(DBusMessage* message) {
  PowerManagementPolicy policy;
  if (!util::ParseProtocolBufferFromDBusMessage(message, &policy)) {
    LOG(WARNING) << "Unable to parse " << kSetPolicyMethod << " request";
    return util::CreateDBusInvalidArgsErrorReply(message);
  }
  state_controller_->HandlePolicyChange(policy);
  return NULL;
}

void Daemon::OnSessionStateChange(const std::string& state_str) {
  SessionState state = (state_str == kSessionStarted) ?
      SESSION_STARTED : SESSION_STOPPED;

  if (state == SESSION_STARTED) {
    DLOG(INFO) << "Session started";

    // We always want to take action even if we were already "started", since we
    // want to record when the current session started.  If this warning is
    // appearing it means either we are querying the state of Session Manager
    // when already know it to be "started" or we missed a "stopped"
    // signal. Both of these cases are bad and should be investigated.
    LOG_IF(WARNING, (session_state_ == state))
        << "Received message saying session started, when we were already in "
        << "the started state!";

    if (!metrics_reporter_->GenerateBatteryRemainingAtStartOfSessionMetric(
            power_supply_->power_status())) {
      LOG(ERROR) << "Unable to generate battery remaining metric";
    }

    if (plugged_state_ == PLUGGED_STATE_DISCONNECTED)
      metrics_reporter_->IncrementNumOfSessionsPerChargeMetric();

    session_start_ = base::TimeTicks::Now();
  } else if (session_state_ != state) {
    DLOG(INFO) << "Session " << state_str;
    // For states other then "started" we only want to take action if we have
    // actually changed state, since the code we are calling assumes that we are
    // actually transitioning between states.
    if (state_str == kSessionStopped) {
      // Don't generate metrics for intermediate states, e.g. "stopping".
      metrics_reporter_->GenerateEndOfSessionMetrics(
          power_supply_->power_status(), base::TimeTicks::Now(),
          session_start_);
    }
  }

  session_state_ = state;
  if (state_controller_initialized_)
    state_controller_->HandleSessionStateChange(state);
  if (backlight_controller_)
    backlight_controller_->HandleSessionStateChange(state);
  if (keyboard_controller_)
    keyboard_controller_->HandleSessionStateChange(state);
}

void Daemon::ShutDown(ShutdownMode mode, const std::string& reason) {
  if (shutting_down_) {
    LOG(WARNING) << "Shutdown already initiated";
    return;
  }

  shutting_down_ = true;
  suspender_.HandleShutdown();

  // If we want to display a low-battery alert while shutting down, don't turn
  // the screen off immediately.
  if (reason != kShutdownReasonLowBattery) {
    if (backlight_controller_)
      backlight_controller_->SetShuttingDown(true);
    if (keyboard_controller_)
      keyboard_controller_->SetShuttingDown(true);
  }

  switch (mode) {
    case SHUTDOWN_POWER_OFF:
      LOG(INFO) << "Shutting down, reason: " << reason;
      util::RunSetuidHelper("shut_down", "--shutdown_reason=" + reason, false);
      break;
    case SHUTDOWN_REBOOT:
      LOG(INFO) << "Restarting";
      util::RunSetuidHelper("reboot", "", false);
      break;
  }
}

void Daemon::Suspend() {
  if (shutting_down_) {
    LOG(INFO) << "Ignoring request for suspend with outstanding shutdown";
    return;
  }
  suspender_.RequestSuspend();
}

void Daemon::SetBacklightsDimmedForInactivity(bool dimmed) {
  if (backlight_controller_)
    backlight_controller_->SetDimmedForInactivity(dimmed);
  if (keyboard_controller_)
    keyboard_controller_->SetDimmedForInactivity(dimmed);
  metrics_reporter_->HandleScreenDimmedChange(
      dimmed, state_controller_->last_user_activity_time());
}

void Daemon::SetBacklightsOffForInactivity(bool off) {
  if (backlight_controller_)
    backlight_controller_->SetOffForInactivity(off);
  if (keyboard_controller_)
    keyboard_controller_->SetOffForInactivity(off);
  metrics_reporter_->HandleScreenOffChange(
      off, state_controller_->last_user_activity_time());
}

void Daemon::SetBacklightsSuspended(bool suspended) {
  if (backlight_controller_)
    backlight_controller_->SetSuspended(suspended);
  if (keyboard_controller_)
    keyboard_controller_->SetSuspended(suspended);
}

void Daemon::SetBacklightsDocked(bool docked) {
  if (backlight_controller_)
    backlight_controller_->SetDocked(docked);
  if (keyboard_controller_)
    keyboard_controller_->SetDocked(docked);
}

}  // namespace power_manager
