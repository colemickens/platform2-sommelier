// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd.h"

#include <cmath>
#include <gdk/gdkx.h>
#include <glib-object.h>
#include <stdint.h>
#include <sys/inotify.h>
#include <X11/extensions/dpms.h>
#include <X11/keysym.h>
#include <X11/XF86keysym.h>

#include <algorithm>
#include <vector>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "chromeos/dbus/dbus.h"
#include "chromeos/dbus/service_constants.h"
#include "chromeos/glib/object.h"
#include "cros/chromeos_wm_ipc_enums.h"
#include "power_manager/audio_detector_interface.h"
#include "power_manager/metrics_constants.h"
#include "power_manager/monitor_reconfigure.h"
#include "power_manager/power_button_handler.h"
#include "power_manager/power_constants.h"
#include "power_manager/power_supply.h"
#include "power_manager/video_detector_interface.h"
#include "power_manager/util.h"

using std::map;
using std::max;
using std::min;
using std::string;
using std::vector;

namespace {

// Path for storing FileTagger files.
const char kTaggedFilePath[] = "/var/lib/power_manager";

// Path to power supply info.
const char kPowerStatusPath[] = "/sys/class/power_supply";

// Power supply subsystem for udev events.
const char kPowerSupplyUdevSubsystem[] = "power_supply";

// Time between battery polls, in milliseconds.
const int64 kBatteryPollIntervalMs = 30000;

} // namespace

namespace power_manager {

// Timeouts are multiplied by this factor when projecting to external display.
static const int64 kProjectionTimeoutFactor = 2;

// Constants for brightness adjustment metric reporting.
enum { kBrightnessDown, kBrightnessUp, kBrightnessEnumMax };

// Daemon: Main power manager. Adjusts device status based on whether the
//         user is idle and on video activity indicator from the window manager.
//         This daemon is responsible for dimming of the backlight, turning
//         the screen off, and suspending to RAM. The daemon also has the
//         capability of shutting the system down.

Daemon::Daemon(BacklightController* backlight_controller,
               PowerPrefs* prefs,
               MetricsLibraryInterface* metrics_lib,
               VideoDetectorInterface* video_detector,
               AudioDetectorInterface* audio_detector,
               MonitorReconfigure* monitor_reconfigure,
               const FilePath& run_dir)
    : backlight_controller_(backlight_controller),
      prefs_(prefs),
      metrics_lib_(metrics_lib),
      video_detector_(video_detector),
      audio_detector_(audio_detector),
      monitor_reconfigure_(monitor_reconfigure),
      low_battery_suspend_percent_(0),
      clean_shutdown_initiated_(false),
      low_battery_(false),
      enforce_lock_(false),
      use_xscreensaver_(false),
      plugged_state_(kPowerUnknown),
      file_tagger_(FilePath(kTaggedFilePath)),
      shutdown_state_(kShutdownNone),
      suspender_(&locker_, &file_tagger_),
      run_dir_(run_dir),
      power_supply_(FilePath(kPowerStatusPath)),
      power_button_handler_(new PowerButtonHandler(this)),
      battery_discharge_rate_metric_last_(0),
      battery_remaining_charge_metric_last_(0),
      battery_time_to_empty_metric_last_(0),
      udev_(NULL) {}

Daemon::~Daemon() {
  if (udev_)
    udev_unref(udev_);
}

void Daemon::Init() {
  ReadSettings();
  CHECK(idle_.Init(this));
  prefs_->StartPrefWatching(&(PrefChangeHandler), this);
  MetricInit();
  if (!DPMSCapable(GDK_DISPLAY())) {
    LOG(WARNING) << "X Server not DPMS capable";
  } else {
    CHECK(DPMSEnable(GDK_DISPLAY()));
    CHECK(DPMSSetTimeouts(GDK_DISPLAY(), 0, 0, 0));
  }
  CHECK(XSetScreenSaver(GDK_DISPLAY(),
                        0,                 // 0 display timeout
                        0,                 // 0 alteration timeout
                        DefaultBlanking,
                        DefaultExposures));
  key_brightness_up_ = XKeysymToKeycode(GDK_DISPLAY(), XF86XK_MonBrightnessUp);
  key_brightness_down_ = XKeysymToKeycode(GDK_DISPLAY(),
                                          XF86XK_MonBrightnessDown);
  key_f6_ = XKeysymToKeycode(GDK_DISPLAY(), XK_F6);
  key_f7_ = XKeysymToKeycode(GDK_DISPLAY(), XK_F7);
  CHECK(key_f6_ != 0);
  CHECK(key_f7_ != 0);
  if (key_brightness_up_ == 0) {
    LOG(ERROR) << "No brightness up keycode found. Guessing instead.";
    key_brightness_up_ = 212;
  }
  if (key_brightness_down_ == 0) {
    LOG(ERROR) << "No brightness down keycode found. Guessing instead.";
    key_brightness_down_ = 101;
  }
  GrabKey(key_brightness_up_, 0);
  GrabKey(key_brightness_down_, 0);
  GrabKey(key_f6_, 0);
  GrabKey(key_f7_, 0);
  gdk_window_add_filter(NULL, GdkEventFilterThunk, this);
  locker_.Init(use_xscreensaver_, lock_on_idle_suspend_);
  RegisterUdevEventHandler();
  RegisterDBusMessageHandler();
  RetrieveSessionState();
  suspender_.Init(run_dir_);
  power_supply_.Init();
  power_supply_.GetPowerStatus(&power_status_);
  OnPowerEvent(this, power_status_);
  file_tagger_.Init();
  backlight_controller_->SetMinimumBrightness(min_backlight_percent_);
  monitor_reconfigure_->SetProjectionCallback(
      &AdjustIdleTimeoutsForProjectionThunk, this);
}

void Daemon::ReadSettings() {
  int64 use_xscreensaver, enforce_lock;
  int64 disable_idle_suspend;
  int64 low_battery_suspend_percent;
  CHECK(prefs_->GetInt64(kLowBatterySuspendPercent,
                         &low_battery_suspend_percent));
  CHECK(prefs_->GetInt64(kCleanShutdownTimeoutMs,
                         &clean_shutdown_timeout_ms_));
  CHECK(prefs_->GetInt64(kPluggedDimMs, &plugged_dim_ms_));
  CHECK(prefs_->GetInt64(kPluggedOffMs, &plugged_off_ms_));
  CHECK(prefs_->GetInt64(kPluggedSuspendMs, &plugged_suspend_ms_));
  CHECK(prefs_->GetInt64(kUnpluggedDimMs, &unplugged_dim_ms_));
  CHECK(prefs_->GetInt64(kUnpluggedOffMs, &unplugged_off_ms_));
  CHECK(prefs_->GetInt64(kUnpluggedSuspendMs, &unplugged_suspend_ms_));
  CHECK(prefs_->GetInt64(kReactMs, &react_ms_));
  CHECK(prefs_->GetInt64(kFuzzMs, &fuzz_ms_));
  CHECK(prefs_->GetInt64(kEnforceLock, &enforce_lock));
  CHECK(prefs_->GetInt64(kUseXScreenSaver, &use_xscreensaver));
  if (prefs_->GetInt64(kDisableIdleSuspend, &disable_idle_suspend) &&
      disable_idle_suspend) {
    LOG(INFO) << "Idle suspend feature disabled";
    plugged_suspend_ms_ = INT64_MAX;
    unplugged_suspend_ms_ = INT64_MAX;
  }
  if (!prefs_->GetInt64(kMinBacklightPercent, &min_backlight_percent_)) {
    LOG(INFO) << "Backlight minimum file not found, setting to default.";
    min_backlight_percent_ = 0;
  }
  ReadLockScreenSettings();
  if (low_battery_suspend_percent >= 0 && low_battery_suspend_percent <= 100) {
    low_battery_suspend_percent_ = low_battery_suspend_percent;
  } else {
    LOG(INFO) << "Unreasonable low battery suspend percent threshold:"
              << low_battery_suspend_percent;
    LOG(INFO) << "Disabling low battery suspend.";
    low_battery_suspend_percent_ = 0;
  }
  lock_ms_ = default_lock_ms_;
  enforce_lock_ = enforce_lock;
  use_xscreensaver_ = use_xscreensaver;

  // Check that timeouts are sane.
  CHECK(kMetricIdleMin >= fuzz_ms_);
  CHECK(plugged_dim_ms_ >= react_ms_);
  CHECK(plugged_off_ms_ >= plugged_dim_ms_ + react_ms_);
  CHECK(plugged_suspend_ms_ >= plugged_off_ms_ + react_ms_);
  CHECK(unplugged_dim_ms_ >= react_ms_);
  CHECK(unplugged_off_ms_ >= unplugged_dim_ms_ + react_ms_);
  CHECK(unplugged_suspend_ms_ >= unplugged_off_ms_ + react_ms_);
  CHECK(default_lock_ms_ >= unplugged_off_ms_ + react_ms_);
  CHECK(default_lock_ms_ >= plugged_off_ms_ + react_ms_);

  // Store unmodified timeout values for switching between projecting and non-
  // projecting timeouts.
  base_timeout_values_[kPluggedDimMs]       = plugged_dim_ms_;
  base_timeout_values_[kPluggedOffMs]       = plugged_off_ms_;
  base_timeout_values_[kPluggedSuspendMs]   = plugged_suspend_ms_;
  base_timeout_values_[kUnpluggedDimMs]     = unplugged_dim_ms_;
  base_timeout_values_[kUnpluggedOffMs]     = unplugged_off_ms_;
  base_timeout_values_[kUnpluggedSuspendMs] = unplugged_suspend_ms_;

  if (monitor_reconfigure_->is_projecting())
    AdjustIdleTimeoutsForProjection();
}

void Daemon::ReadLockScreenSettings() {
  int64 lock_on_idle_suspend;
  if (prefs_->GetInt64(kLockOnIdleSuspend, &lock_on_idle_suspend) &&
      0 == lock_on_idle_suspend) {
    LOG(INFO) << "Disabling screen lock on idle and suspend";
    default_lock_ms_ = INT64_MAX;
  } else {
    CHECK(prefs_->GetInt64(kLockMs, &default_lock_ms_));
    LOG(INFO) << "Enabling screen lock on idle and suspend";
  }
  lock_on_idle_suspend_ = lock_on_idle_suspend;
}

void Daemon::Run() {
  GMainLoop* loop = g_main_loop_new(NULL, false);
  g_timeout_add(kBatteryPollIntervalMs, PollPowerSupplyThunk, this);
  g_main_loop_run(loop);
}

void Daemon::SetPlugged(bool plugged) {
  if (plugged == plugged_state_)
    return;

  LOG(INFO) << "Daemon : SetPlugged = " << plugged;
  plugged_state_ = plugged ? kPowerConnected : kPowerDisconnected;
  int64 idle_time_ms;
  CHECK(idle_.GetIdleTime(&idle_time_ms));
  // If the screen is on, and the user plugged or unplugged the computer,
  // we should wait a bit before turning off the screen.
  // If the screen is off, don't immediately suspend, wait another
  // suspend timeout.
  // If the state is uninitialized, this is the powerd startup condition, so
  // we ignore any idle time from before powerd starts.
  if (backlight_controller_->state() == BACKLIGHT_ACTIVE ||
      backlight_controller_->state() == BACKLIGHT_DIM)
    SetIdleOffset(idle_time_ms, kIdleNormal);
  else if (backlight_controller_->state() == BACKLIGHT_IDLE_OFF)
    SetIdleOffset(idle_time_ms, kIdleSuspend);
  else if (backlight_controller_->state() == BACKLIGHT_UNINITIALIZED)
    SetIdleOffset(idle_time_ms, kIdleNormal);
  else
    SetIdleOffset(0, kIdleNormal);

  if (backlight_controller_->OnPlugEvent(plugged))
    SendBrightnessChangedSignal(false);  // user_initiated=false
  SetIdleState(idle_time_ms);
}

void Daemon::OnRequestRestart(bool notify_window_manager) {
  if (shutdown_state_ == kShutdownNone) {
    if (notify_window_manager) {
      util::SendMessageToWindowManager(
          chromeos::WM_IPC_MESSAGE_WM_NOTIFY_SHUTTING_DOWN, 0);
    }
    shutdown_state_ = kShutdownRestarting;
    StartCleanShutdown();
  }
}

void Daemon::OnRequestShutdown(bool notify_window_manager) {
  if (shutdown_state_ == kShutdownNone) {
    if (notify_window_manager) {
      util::SendMessageToWindowManager(
          chromeos::WM_IPC_MESSAGE_WM_NOTIFY_SHUTTING_DOWN, 0);
    }
    shutdown_state_ = kShutdownPowerOff;
    StartCleanShutdown();
  }
}

void Daemon::StartCleanShutdown() {
  clean_shutdown_initiated_ = true;
  // Cancel any outstanding suspend in flight.
  suspender_.CancelSuspend();
  util::SendSignalToPowerM(util::kRequestCleanShutdown);
  g_timeout_add(clean_shutdown_timeout_ms_, CleanShutdownTimedOutThunk, this);
}

void Daemon::SetIdleOffset(int64 offset_ms, IdleState state) {
  AdjustIdleTimeoutsForProjection();
  int64 prev_dim_ms = dim_ms_;
  int64 prev_off_ms = off_ms_;
  LOG(INFO) << "offset_ms_ = " << offset_ms;
  offset_ms_ = offset_ms;
  if (plugged_state_ == kPowerConnected) {
    dim_ms_ = plugged_dim_ms_;
    off_ms_ = plugged_off_ms_;
    suspend_ms_ = plugged_suspend_ms_;
  } else {
    CHECK(plugged_state_ == kPowerDisconnected);
    dim_ms_ = unplugged_dim_ms_;
    off_ms_ = unplugged_off_ms_;
    suspend_ms_ = unplugged_suspend_ms_;
  }
  lock_ms_ = default_lock_ms_;

  // Protect against overflow
  dim_ms_ = max(dim_ms_ + offset_ms, dim_ms_);
  off_ms_ = max(off_ms_ + offset_ms, off_ms_);
  suspend_ms_ = max(suspend_ms_ + offset_ms, suspend_ms_);

  if (enforce_lock_) {
    // Make sure that the screen turns off before it locks, and dims before
    // it turns off. This ensures the user gets a warning before we lock the
    // screen.
    off_ms_ = min(off_ms_, lock_ms_ - react_ms_);
    dim_ms_ = min(dim_ms_, lock_ms_ - 2 * react_ms_);
  } else {
    lock_ms_ = max(lock_ms_ + offset_ms, lock_ms_);
  }

  // Only offset timeouts for states starting with idle state provided.
  switch (state) {
    case kIdleSuspend:
      off_ms_ = prev_off_ms;
    case kIdleScreenOff:
      dim_ms_ = prev_dim_ms;
    case kIdleDim:
    case kIdleNormal:
      break;
    case kIdleUnknown:
    default: {
      LOG(ERROR) << "SetIdleOffset : Improper Idle State";
      break;
    }
  }

  // Sync up idle state with new settings.
  CHECK(idle_.ClearTimeouts());
  if (offset_ms > fuzz_ms_)
    CHECK(idle_.AddIdleTimeout(fuzz_ms_));
  if (kMetricIdleMin <= dim_ms_ - fuzz_ms_)
    CHECK(idle_.AddIdleTimeout(kMetricIdleMin));
  CHECK(idle_.AddIdleTimeout(dim_ms_));
  CHECK(idle_.AddIdleTimeout(off_ms_));
  if (lock_ms_ < suspend_ms_ - fuzz_ms_ || lock_ms_ - fuzz_ms_ > suspend_ms_) {
    CHECK(idle_.AddIdleTimeout(lock_ms_));
    CHECK(idle_.AddIdleTimeout(suspend_ms_));
  } else {
    CHECK(idle_.AddIdleTimeout(max(lock_ms_, suspend_ms_)));
  }
}

// SetActive will transition to Normal state. Used for transitioning on events
// that do not result in activity monitored by X, i.e. lid open.
void Daemon::SetActive() {
  int64 idle_time_ms;
  CHECK(idle_.GetIdleTime(&idle_time_ms));
  SetIdleOffset(idle_time_ms, kIdleNormal);
  SetIdleState(idle_time_ms);
}

void Daemon::OnIdleEvent(bool is_idle, int64 idle_time_ms) {
  CHECK(plugged_state_ != kPowerUnknown);
  if (is_idle && backlight_controller_->state() == BACKLIGHT_ACTIVE &&
      dim_ms_ <= idle_time_ms && !locker_.is_locked()) {
    int64 video_time_ms = 0;
    bool video_is_playing = false;
    int64 dim_timeout = kPowerConnected == plugged_state_ ? plugged_dim_ms_ :
                                                            unplugged_dim_ms_;
    CHECK(video_detector_->GetVideoActivity(dim_timeout,
                                            &video_time_ms,
                                            &video_is_playing));
    if (video_is_playing)
      SetIdleOffset(idle_time_ms - video_time_ms, kIdleNormal);
  }
  if (is_idle && backlight_controller_->state() == BACKLIGHT_DIM &&
      !util::OOBECompleted()) {
    LOG(INFO) << "OOBE not complete. Delaying screenoff until done.";
    SetIdleOffset(idle_time_ms, kIdleScreenOff);
  }
  if (is_idle && backlight_controller_->state() == BACKLIGHT_IDLE_OFF &&
      idle_time_ms >= suspend_ms_) {
    // Delay suspend if audio is active.
    bool audio_is_playing = false;
    // TODO(sque): Add a CHECK once AudioDetector is more flexible and supports
    // various audio sysfs across different systems.
    audio_detector_->GetAudioStatus(&audio_is_playing);
    if (audio_is_playing) {
      LOG(INFO) << "Delaying suspend because audio is playing.";
      // Increase the suspend offset by the react time.  Since the offset is
      // calculated relative to the ORIGINAL [un]plugged_suspend_ms_ value, we
      // need to use that here.
      int64 base_suspend_ms = (plugged_state_ == kPowerConnected) ?
                               plugged_suspend_ms_ : unplugged_suspend_ms_;
      SetIdleOffset(suspend_ms_ - base_suspend_ms + react_ms_, kIdleSuspend);
    }
  }

  GenerateMetricsOnIdleEvent(is_idle, idle_time_ms);
  SetIdleState(idle_time_ms);
  if (!is_idle && offset_ms_ != 0)
    SetIdleOffset(0, kIdleNormal);
}

void Daemon::SetIdleState(int64 idle_time_ms) {
  bool changed_brightness = false;
  if (idle_time_ms >= suspend_ms_) {
    // Note: currently this state doesn't do anything.  But it can be possibly
    // useful in future development.  For example, if we want to implement
    // fade from suspend, we would want to have this state to make sure the
    // backlight is set to zero when suspended.
    changed_brightness =
        backlight_controller_->SetPowerState(BACKLIGHT_SUSPENDED);
    Suspend();
  } else if (idle_time_ms >= off_ms_) {
    if (util::LoggedIn() &&
        backlight_controller_->SetPowerState(BACKLIGHT_IDLE_OFF)) {
      changed_brightness = true;
    }
  } else if (idle_time_ms >= dim_ms_) {
    if (backlight_controller_->SetPowerState(BACKLIGHT_DIM))
      changed_brightness = true;
  } else if (backlight_controller_->state() != BACKLIGHT_ACTIVE) {
    if (backlight_controller_->SetPowerState(BACKLIGHT_ACTIVE)) {
      if (backlight_controller_->state() == BACKLIGHT_SUSPENDED) {
        util::CreateStatusFile(FilePath(run_dir_)
                              .Append(util::kUserActiveFile));
        suspender_.CancelSuspend();
      }
      changed_brightness = true;
    }
  } else if (idle_time_ms < react_ms_ && locker_.is_locked()) {
    BrightenScreenIfOff();
  }
  if (idle_time_ms >= lock_ms_ && util::LoggedIn() &&
      backlight_controller_->state() != BACKLIGHT_SUSPENDED) {
    locker_.LockScreen();
  }

  if (changed_brightness)
    SendBrightnessChangedSignal(false);  // user_initiated=false
}

void Daemon::OnPowerEvent(void* object, const chromeos::PowerStatus& info) {
  Daemon* daemon = static_cast<Daemon*>(object);
  daemon->SetPlugged(info.line_power_on);
  daemon->GenerateMetricsOnPowerEvent(info);
  // Do not emergency suspend if no battery exists.
  if (info.battery_is_present)
    daemon->OnLowBattery(info.battery_percentage);
}

GdkFilterReturn Daemon::GdkEventFilter(GdkXEvent* gxevent, GdkEvent*) {
  XEvent* xevent = static_cast<XEvent*>(gxevent);

  if (xevent->type == KeyPress)
    LOG(INFO) << backlight_controller_->state();
  if (xevent->type == KeyPress &&
      (backlight_controller_->state() == BACKLIGHT_ACTIVE ||
       backlight_controller_->state() == BACKLIGHT_ALREADY_DIMMED)) {
    SetIdleState(0);
    int keycode = xevent->xkey.keycode;
    if (keycode == key_brightness_up_ || keycode == key_f7_) {
      if (keycode == key_brightness_up_) {
        LOG(INFO) << "Key press: Brightness up";
      } else {  // keycode == key_f7_
        LOG(INFO) << "Key press: F7";
        metrics_lib_->SendUserActionToUMA("Accel_BrightnessUp_F7");
      }
      IncreaseScreenBrightness(true);
      SendEnumMetricWithPowerState(kMetricBrightnessAdjust, kBrightnessUp,
                                   kBrightnessEnumMax);
    } else if (keycode == key_brightness_down_ || keycode == key_f6_) {
      if (keycode == key_brightness_down_) {
        LOG(INFO) << "Key press: Brightness down";
      } else {  // keycode == key_f6_
        LOG(INFO) << "Key press: F6";
        metrics_lib_->SendUserActionToUMA("Accel_BrightnessDown_F6");
      }
      DecreaseScreenBrightness(true, true);
      SendEnumMetricWithPowerState(kMetricBrightnessAdjust, kBrightnessDown,
                                   kBrightnessEnumMax);
    }
  }

  return GDK_FILTER_CONTINUE;
}

void Daemon::DecreaseScreenBrightness(bool allow_off, bool user_initiated) {
  backlight_controller_->DecreaseBrightness(allow_off);
  SendBrightnessChangedSignal(user_initiated);
}

void Daemon::IncreaseScreenBrightness(bool user_initiated) {
  backlight_controller_->IncreaseBrightness();
  SendBrightnessChangedSignal(user_initiated);
}

void Daemon::BrightenScreenIfOff() {
  if (util::LoggedIn() && backlight_controller_->IsBacklightActiveOff())
    backlight_controller_->IncreaseBrightness();
}

void Daemon::GrabKey(KeyCode key, uint32 mask) {
  uint32 NumLockMask = Mod2Mask;
  uint32 CapsLockMask = LockMask;
  Window root = DefaultRootWindow(GDK_DISPLAY());
  XGrabKey(GDK_DISPLAY(), key, mask, root, True, GrabModeAsync, GrabModeAsync);
  XGrabKey(GDK_DISPLAY(), key, mask | CapsLockMask, root, True,
           GrabModeAsync, GrabModeAsync);
  XGrabKey(GDK_DISPLAY(), key, mask | NumLockMask, root, True,
           GrabModeAsync, GrabModeAsync);
  XGrabKey(GDK_DISPLAY(), key, mask | CapsLockMask | NumLockMask, root, True,
           GrabModeAsync, GrabModeAsync);
}

gboolean Daemon::UdevEventHandler(GIOChannel* /* source */,
                                  GIOCondition /* condition */,
                                  gpointer data) {
  Daemon* daemon = static_cast<Daemon*>(data);

  struct udev_device* dev = udev_monitor_receive_device(daemon->udev_monitor_);
  if (dev) {
    LOG(INFO) << "Event on ("
              << udev_device_get_subsystem(dev)
              << ") Action "
              << udev_device_get_action(dev);
    CHECK(string(udev_device_get_subsystem(dev)) == kPowerSupplyUdevSubsystem);
    udev_device_unref(dev);

    daemon->PollPowerSupply();
  } else {
    LOG(ERROR) << "Can't get receive_device()";
    return false;
  }
  return true;
}

void Daemon::RegisterUdevEventHandler() {
  // Create the udev object.
  udev_ = udev_new();
  if (!udev_)
    LOG(ERROR) << "Can't create udev object.";

  // Create the udev monitor structure.
  udev_monitor_ = udev_monitor_new_from_netlink(udev_, "udev");
  if (!udev_monitor_ ) {
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

DBusHandlerResult Daemon::DBusMessageHandler(DBusConnection* connection,
                                             DBusMessage* message,
                                             void* data) {
  Daemon* daemon = static_cast<Daemon*>(data);
  if (dbus_message_is_signal(message, kPowerManagerInterface,
                             kRequestLockScreenSignal)) {
    LOG(INFO) << "RequestLockScreen event";
    daemon->locker_.LockScreen();
  } else if (dbus_message_is_signal(message, kPowerManagerInterface,
                                    kRequestUnlockScreenSignal)) {
    LOG(INFO) << "RequestUnlockScreen event";
    util::SendSignalToSessionManager("UnlockScreen");
  } else if (dbus_message_is_signal(message, kPowerManagerInterface,
                                    kScreenIsLockedSignal)) {
    LOG(INFO) << "ScreenIsLocked event";
    daemon->locker_.set_locked(true);
    daemon->suspender_.CheckSuspend();
  } else if (dbus_message_is_signal(message, kPowerManagerInterface,
                                    kScreenIsUnlockedSignal)) {
    LOG(INFO) << "ScreenIsUnlocked event";
    daemon->locker_.set_locked(false);
  } else if (dbus_message_is_signal(message, kPowerManagerInterface,
                                    kRequestSuspendSignal)) {
    LOG(INFO) << "RequestSuspend event";
    daemon->Suspend();
  } else if (dbus_message_is_signal(message, kPowerManagerInterface,
                                    kRequestRestartSignal)) {
    LOG(INFO) << "RequestRestart event";
    daemon->OnRequestRestart(true);  // notify_window_manager=true
  } else if (dbus_message_is_signal(message, kPowerManagerInterface,
                                    kRequestShutdownSignal)) {
    LOG(INFO) << "RequestShutdown event";
    daemon->OnRequestShutdown(true);  // notify_window_manager=true
  } else if (dbus_message_is_signal(message, kPowerManagerInterface,
                                    util::kLidClosed)) {
    LOG(INFO) << "Lid Closed event";
    daemon->SetActive();
    daemon->Suspend();
  } else if (dbus_message_is_signal(message, kPowerManagerInterface,
                                    util::kLidOpened)) {
    LOG(INFO) << "Lid Opened event";
    daemon->SetActive();
    daemon->suspender_.CancelSuspend();
  } else if (dbus_message_is_signal(message, kPowerManagerInterface,
                                    util::kPowerButtonDown)) {
    LOG(INFO) << "Button Down event";
    daemon->power_button_handler_->HandleButtonDown();
  } else if (dbus_message_is_signal(message, kPowerManagerInterface,
                                    util::kPowerButtonUp)) {
    LOG(INFO) << "Button Up event";
    daemon->power_button_handler_->HandleButtonUp();
  } else if (dbus_message_is_signal(message, kPowerManagerInterface,
                                    kCleanShutdown)) {
    LOG(INFO) << "Clean shutdown/restart event";
    if (daemon->clean_shutdown_initiated_) {
      daemon->clean_shutdown_initiated_ = false;
      daemon->Shutdown();
    } else {
      LOG(INFO) << "Received clean shutdown signal, but never asked for it.";
    }
  } else if (dbus_message_is_signal(message, kPowerManagerInterface,
                                    util::kPowerStateChanged)) {
    LOG(INFO) << "Power state change event";
    const char* state = '\0';
    DBusError error;
    dbus_error_init(&error);
    if (dbus_message_get_args(message, &error, DBUS_TYPE_STRING, &state,
                              DBUS_TYPE_INVALID) == FALSE) {
      LOG(WARNING) << "Trouble reading args of PowerStateChange event "
                   << state;
      return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }
    daemon->OnPowerStateChange(state);
    // Other dbus clients may be interested in consuming this signal.
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  } else if (dbus_message_is_signal(message, kPowerManagerInterface,
                                    "PowerSupplyChange")) {
    LOG(INFO) << "Power supply change event";
    daemon->PollPowerSupply();
    // Other dbus clients may be interested in consuming this signal.
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  } else if (dbus_message_is_method_call(message,
                                         kPowerManagerInterface,
                                         "GetAllProperties")) {
    LOG(INFO) << "Power supply all properties request";

    DBusMessage *reply = dbus_message_new_method_return(message);
    CHECK(reply != NULL);
    dbus_message_append_args(reply,
        DBUS_TYPE_BOOLEAN, &daemon->power_status_.line_power_on,
        DBUS_TYPE_DOUBLE,  &daemon->power_status_.battery_energy,
        DBUS_TYPE_DOUBLE,  &daemon->power_status_.battery_energy_rate,
        DBUS_TYPE_DOUBLE,  &daemon->power_status_.battery_voltage,
        DBUS_TYPE_INT64,   &daemon->power_status_.battery_time_to_empty,
        DBUS_TYPE_INT64,   &daemon->power_status_.battery_time_to_full,
        DBUS_TYPE_DOUBLE,  &daemon->power_status_.battery_percentage,
        DBUS_TYPE_BOOLEAN, &daemon->power_status_.battery_is_present,
        DBUS_TYPE_INT32,   &daemon->power_status_.battery_state,
        DBUS_TYPE_INVALID);

    if (!dbus_connection_send(connection, reply, NULL))
      LOG(WARNING) << "Failed to send reply message.";
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  } else if (dbus_message_is_method_call(message,
                                         kPowerManagerInterface,
                                         "GetProperty")) {
    LOG(INFO) << "Power supply property request";
    DBusError error;
    dbus_error_init(&error);
    char *property_name = NULL;
    if (dbus_message_get_args(message, &error,
                              DBUS_TYPE_STRING, &property_name,
                              DBUS_TYPE_INVALID) == FALSE) {
      LOG(WARNING) << "Failed reading args from Get method call: "
                   << error.message;
      return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    std::string property = property_name;
    int dbus_type;
    union {
      double double_value;
      bool bool_value;
      int64 int64_value;
      int32 int32_value;
    } power_value;

    // Determine type and value of the property being requested.
    if (property == "line_power_on") {
      power_value.bool_value = daemon->power_status_.line_power_on;
      dbus_type = DBUS_TYPE_BOOLEAN;
    } else if (property == "battery_energy") {
      power_value.double_value = daemon->power_status_.battery_energy;
      dbus_type = DBUS_TYPE_DOUBLE;
    } else if (property == "battery_energy_rate") {
      power_value.double_value = daemon->power_status_.battery_energy_rate;
      dbus_type = DBUS_TYPE_DOUBLE;
    } else if (property == "battery_voltage") {
      power_value.double_value = daemon->power_status_.battery_voltage;
      dbus_type = DBUS_TYPE_DOUBLE;
    } else if (property == "battery_time_to_empty") {
      power_value.int64_value = daemon->power_status_.battery_time_to_empty;
      dbus_type = DBUS_TYPE_INT64;
    } else if (property == "battery_time_to_full") {
      power_value.int64_value = daemon->power_status_.battery_time_to_full;
      dbus_type = DBUS_TYPE_INT64;
    } else if (property == "battery_percentage") {
      power_value.double_value = daemon->power_status_.battery_percentage;
      dbus_type = DBUS_TYPE_DOUBLE;
    } else if (property == "battery_is_present") {
      power_value.bool_value = daemon->power_status_.battery_is_present;
      dbus_type = DBUS_TYPE_BOOLEAN;
    } else if (property == "battery_state") {
      power_value.int32_value = daemon->power_status_.battery_state;
      dbus_type = DBUS_TYPE_INT32;
    } else {
      return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }
    DBusMessage *reply = dbus_message_new_method_return(message);
    CHECK(reply != NULL);
    dbus_message_append_args(reply, dbus_type, &power_value, DBUS_TYPE_INVALID);
    if (!dbus_connection_send(connection, reply, NULL))
      LOG(WARNING) << "Failed to send reply message.";

    // Other dbus clients may be interested in consuming this signal.
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  } else if (dbus_message_is_signal(
                 message,
                 login_manager::kSessionManagerInterface,
                 login_manager::kSessionManagerSessionStateChanged)) {
    DBusError error;
    dbus_error_init(&error);
    const char* state = NULL;
    const char* user = NULL;
    if (dbus_message_get_args(message, &error,
                              DBUS_TYPE_STRING, &state,
                              DBUS_TYPE_STRING, &user,
                              DBUS_TYPE_INVALID)) {
      daemon->OnSessionStateChange(state, user);
    } else {
      LOG(WARNING) << "Unable to read arguments from "
                   << login_manager::kSessionManagerSessionStateChanged
                   << " signal";
    }
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  } else {
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }
  return DBUS_HANDLER_RESULT_HANDLED;
}

void Daemon::RegisterDBusMessageHandler() {
  DBusConnection* connection = dbus_g_connection_get_connection(
      chromeos::dbus::GetSystemBusConnection().g_connection());
  CHECK(connection);

  vector<string> matches;
  matches.push_back(
      StringPrintf("type='signal', interface='%s'", kPowerManagerInterface));
  matches.push_back(
      StringPrintf("type='signal', interface='%s', member='%s'",
                   login_manager::kSessionManagerInterface,
                   login_manager::kSessionManagerSessionStateChanged));
  matches.push_back(
      StringPrintf("type='method_call', interface='%s', path='%s'",
                   kPowerManagerInterface,
                   kPowerManagerServicePath));

  for (vector<string>::const_iterator it = matches.begin();
       it != matches.end(); ++it) {
    DBusError error;
    dbus_error_init(&error);
    dbus_bus_add_match(connection, it->c_str(), &error);
    if (dbus_error_is_set(&error)) {
      LOG(DFATAL) << "Failed to add match \"" << *it << "\": "
                  << error.name << ", message=" << error.message;
    }
  }

  CHECK(dbus_connection_add_filter(connection, &DBusMessageHandler, this,
                                   NULL));
  LOG(INFO) << "D-Bus monitoring started";
}

gboolean Daemon::PollPowerSupply() {
  power_supply_.GetPowerStatus(&power_status_);
  OnPowerEvent(this, power_status_);
  // Send a signal once the power supply status has been obtained.
  DBusMessage* message = dbus_message_new_signal(kPowerManagerServicePath,
                                                 kPowerManagerInterface,
                                                 "PowerSupplyPoll");
  CHECK(message != NULL);
  DBusConnection* connection = dbus_g_connection_get_connection(
      chromeos::dbus::GetSystemBusConnection().g_connection());
  if (!dbus_connection_send(connection, message, NULL))
    LOG(WARNING) << "Sending battery poll signal failed.";
  // Always repeat polling.
  return true;
}

void Daemon::OnLowBattery(double battery_percentage) {
  if (!low_battery_suspend_percent_) {
    LOG(INFO) << "Battery percent : "
              << battery_percentage << "%";
    low_battery_ = false;
    return;
  }
  if (kPowerDisconnected == plugged_state_ && !low_battery_ &&
      battery_percentage <= low_battery_suspend_percent_) {
    // Shut the system down when low battery condition is encountered.
    LOG(INFO) << "Low battery condition detected. Shutting down immediately.";
    low_battery_ = true;
    file_tagger_.HandleLowBatteryEvent();
    OnRequestShutdown(true);  // notify_window_manager=true
  } else if (kPowerConnected == plugged_state_ ||
             battery_percentage > low_battery_suspend_percent_ ) {
    LOG(INFO) << "Battery condition is safe (plugged in or not low) : "
              << battery_percentage << "%";
    low_battery_ = false;
    file_tagger_.HandleSafeBatteryEvent();
  } else {
    // Either a spurious reading after we have requested suspend, or the user
    // has woken the system up intentionally without rectifying the battery
    // situation (ie. user woke the system without attaching AC.)
    // User is on his own from here until the system dies. We will not try to
    // resuspend.
    LOG(INFO) << "Spurious low battery condition, or user living on the edge.";
    file_tagger_.HandleLowBatteryEvent();
  }
}

gboolean Daemon::CleanShutdownTimedOut() {
  if (clean_shutdown_initiated_) {
    clean_shutdown_initiated_ = false;
    LOG(INFO) << "Timed out waiting for clean shutdown/restart.";
    Shutdown();
  } else {
    LOG(INFO) << "Shutdown already handled. clean_shutdown_initiated_ == false";
  }
  return false;
}

void Daemon::OnPowerStateChange(const char* state) {
 // on == resume via powerd_suspend
  if (g_str_equal(state, "on") == TRUE) {
    LOG(INFO) << "Resuming has commenced";
    SetActive();
    HandleResume();
  } else {
    DLOG(INFO) << "Saw arg:" << state << " for PowerStateChange";
  }
}

void Daemon::OnSessionStateChange(const char* state, const char* user) {
  if (!state || !user) {
    LOG(DFATAL) << "Got session state change with missing state or user";
    return;
  }

  if (strcmp(state, "started") == 0) {
    current_user_ = user;
    DLOG(INFO) << "Session started for "
               << (current_user_.empty() ? "guest" : "non-guest user");
  } else if (strcmp(state, "stopped") == 0) {
    current_user_.clear();
    DLOG(INFO) << "Session stopped";
  } else {
    LOG(WARNING) << "Got unexpected state in session state change signal: "
                 << state;
  }
}

void Daemon::Shutdown() {
  if (shutdown_state_ == kShutdownPowerOff) {
    LOG(INFO) << "Shutting down";
    util::SendSignalToPowerM(util::kShutdownSignal);
  } else if (shutdown_state_ == kShutdownRestarting) {
    LOG(INFO) << "Restarting";
    util::SendSignalToPowerM(util::kRestartSignal);
  } else {
    LOG(ERROR) << "Shutdown : Improper System State!";
  }
}

void Daemon::Suspend() {
  if (clean_shutdown_initiated_) {
    LOG(INFO) << "Ignoring request for suspend with outstanding shutdown.";
    return;
  }
  if (util::LoggedIn()) {
    suspender_.RequestSuspend();
  } else {
    LOG(INFO) << "Not logged in. Suspend Request -> Shutting down.";
    OnRequestShutdown(true);  // notify_window_manager=true
  }
}

gboolean Daemon::PrefChangeHandler(const char* name,
                                   int,               // watch handle
                                   unsigned int,      // mask
                                   gpointer data) {
  Daemon* daemon = static_cast<Daemon*>(data);
  if (!strcmp(name, "lock_on_idle_suspend")) {
    daemon->ReadLockScreenSettings();
    daemon->locker_.Init(daemon->use_xscreensaver_,
                         daemon->lock_on_idle_suspend_);
    daemon->SetIdleOffset(0, kIdleNormal);
  }
  return true;
}

void Daemon::SendBrightnessChangedSignal(bool user_initiated) {
  double brightness = 0.;
  int64 brightness_rounded;
  if (!backlight_controller_->GetTargetBrightness(&brightness))
    return;
  brightness_rounded = lround(brightness);

  // DBUS_TYPE_BOOLEAN actually corresponds to an int.
  int user_initiated_int = user_initiated;

  chromeos::dbus::Proxy proxy(chromeos::dbus::GetSystemBusConnection(),
                              "/",
                              kPowerManagerInterface);
  DBusMessage* signal = dbus_message_new_signal("/",
                                                kPowerManagerInterface,
                                                kBrightnessChangedSignal);
  CHECK(signal);
  dbus_message_append_args(signal,
                           DBUS_TYPE_INT32, &brightness_rounded,
                           DBUS_TYPE_BOOLEAN, &user_initiated_int,
                           DBUS_TYPE_INVALID);
  dbus_g_proxy_send(proxy.gproxy(), signal, NULL);
  dbus_message_unref(signal);
}

void Daemon::HandleResume() {
  file_tagger_.HandleResumeEvent();
  // Monitor reconfigure will set the backlight if needed.
  monitor_reconfigure_->Run();
}

void Daemon::RetrieveSessionState() {
  DBusGConnection* connection =
      chromeos::dbus::GetSystemBusConnection().g_connection();
  CHECK(connection);

  DBusGProxy* proxy = dbus_g_proxy_new_for_name(
      connection,
      login_manager::kSessionManagerServiceName,
      login_manager::kSessionManagerServicePath,
      login_manager::kSessionManagerInterface);

  GError* error = NULL;
  gchar* state = NULL;
  gchar* user = NULL;
  if (dbus_g_proxy_call(proxy,
                        login_manager::kSessionManagerRetrieveSessionState,
                        &error,
                        G_TYPE_INVALID,
                        G_TYPE_STRING,
                        &state,
                        G_TYPE_STRING,
                        &user,
                        G_TYPE_INVALID)) {
    OnSessionStateChange(state, user);
    g_free(state);
    g_free(user);
  } else {
    LOG(ERROR) << "Unable to retrieve session state from session manager: "
               << error->message;
    g_error_free(error);
  }
  g_object_unref(proxy);
}

void Daemon::AdjustIdleTimeoutsForProjection() {
  plugged_dim_ms_       = base_timeout_values_[kPluggedDimMs];
  plugged_off_ms_       = base_timeout_values_[kPluggedOffMs];
  plugged_suspend_ms_   = base_timeout_values_[kPluggedSuspendMs];
  unplugged_dim_ms_     = base_timeout_values_[kUnpluggedDimMs];
  unplugged_off_ms_     = base_timeout_values_[kUnpluggedOffMs];
  unplugged_suspend_ms_ = base_timeout_values_[kUnpluggedSuspendMs];

  if (monitor_reconfigure_->is_projecting()) {
    LOG(INFO) << "External display projection: Doubling idle times.";
    plugged_dim_ms_ *= kProjectionTimeoutFactor;
    plugged_off_ms_ *= kProjectionTimeoutFactor;
    if (plugged_suspend_ms_ != INT64_MAX)
      plugged_suspend_ms_ *= kProjectionTimeoutFactor;
    unplugged_dim_ms_ *= kProjectionTimeoutFactor;
    unplugged_off_ms_ *= kProjectionTimeoutFactor;
    if (unplugged_suspend_ms_ != INT64_MAX)
      unplugged_suspend_ms_ *= kProjectionTimeoutFactor;
  }
}

}  // namespace power_manager
