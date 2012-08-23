// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERMAN_H_
#define POWER_MANAGER_POWERMAN_H_

#include <sys/types.h>
#include <dbus/dbus-glib-lowlevel.h>

#include "base/file_path.h"
#include "base/time.h"
#include "metrics/metrics_library.h"
#include "power_manager/input.h"
#include "power_manager/power_prefs.h"
#include "power_manager/powerd.h"

namespace power_manager {

class BacklightInterface;

class PowerManDaemon {
 public:
  PowerManDaemon(PowerPrefs* prefs,
                 MetricsLibraryInterface* metrics_lib,
                 BacklightInterface* backlight,
                 const FilePath& run_dir);
  virtual ~PowerManDaemon();

  void Init();
  void Run();

 private:
  friend class PowerManDaemonTest;
  FRIEND_TEST(PowerManDaemonTest, SendMetric);
  FRIEND_TEST(PowerManDaemonTest, GenerateRetrySuspendCountMetric);

  enum LidState {
    LID_STATE_CLOSED,
    LID_STATE_OPENED,
  };

  inline static LidState GetLidState(int value) {
    // value == 0 is open. value == 1 is closed.
    return value == 0 ? LID_STATE_OPENED : LID_STATE_CLOSED;
  }

  enum ButtonState {
    BUTTON_UP = 0,
    BUTTON_DOWN,
    BUTTON_REPEAT,
  };

  typedef std::pair<std::string, std::string> DBusInterfaceMemberPair;
  typedef void (PowerManDaemon::*DBusSignalHandler)(DBusMessage*);
  typedef DBusMessage* (PowerManDaemon::*DBusMethodHandler)(DBusMessage*);

  typedef std::map<DBusInterfaceMemberPair, DBusSignalHandler>
      DBusSignalHandlerTable;
  typedef std::map<DBusInterfaceMemberPair, DBusMethodHandler>
      DBusMethodHandlerTable;

  inline static ButtonState GetButtonState(int value) {
    // value == 0 is button up.
    // value == 1 is button down.
    // value == 2 is key repeat.
    return static_cast<ButtonState>(value);
  }

  // UMA metrics parameters.
  static const char kMetricRetrySuspendCountName[];
  static const int kMetricRetrySuspendCountMin;
  static const int kMetricRetrySuspendCountBuckets;

  // Handler for input events. |object| contains a pointer to a PowerManDaemon
  // object. |type| contains the event type (lid or power button). |value|
  // contains the new state of this input device.
  static void OnInputEvent(void* object, InputType type, int value);

  // Methods for handling input events.
  void HandlePowerButtonEvent(ButtonState value);

  // Standard handlers for dbus method calls and signals. |data| contains a
  // pointer to a Daemon object.
  static DBusHandlerResult MainDBusMethodHandler(
      DBusConnection*, DBusMessage* message, void* data);
  static DBusHandlerResult MainDBusSignalHandler(
      DBusConnection*, DBusMessage* message, void* data);

  // Callbacks for handling dbus messages.
  void HandleCheckLidStateSignal(DBusMessage* message);
  void HandleSuspendSignal(DBusMessage* message);
  void HandleShutdownSignal(DBusMessage* message);
  void HandleRestartSignal(DBusMessage* message);
  void HandleRequestCleanShutdownSignal(DBusMessage* message);
  void HandlePowerStateChangedSignal(DBusMessage* message);
  void HandleSessionManagerStateChangedSignal(DBusMessage* message);
  DBusMessage* HandleExternalBacklightGetMethod(DBusMessage* message);
  DBusMessage* HandleExternalBacklightSetMethod(DBusMessage* message);
  DBusMessage* HandleDisableTouchDevicesMethod(DBusMessage* message);
  DBusMessage* HandleEnableTouchDevicesMethod(DBusMessage* message);

  void AddDBusSignalHandler(const std::string& interface,
                            const std::string& member,
                            DBusSignalHandler handler);

  void AddDBusMethodHandler(const std::string& interface,
                            const std::string& member,
                            DBusMethodHandler handler);

  bool CancelDBusRequest();

  enum SessionManagerState {
    kSessionStarted,
    kSessionStopping,
    kSessionStopped,
  };

  enum PowerManagerState {
    kPowerManagerUnknown,
    kPowerManagerAlive,
    kPowerManagerDead,
  };

  // Handler for NameOwnerChanged dbus messages.  See dbus-specification
  // at dbus.freedesktop.org for complete details of arguments
  static void DBusNameOwnerChangedHandler(
      DBusGProxy*, const gchar* name, const gchar* old_owner,
      const gchar* new_owner, void*);

  // Callback for timeout event started when lid closed to validate powerd has
  // received it successfully.
  SIGNAL_CALLBACK_PACKED_2(PowerManDaemon, gboolean, CheckLidClosed,
                           unsigned int, unsigned int);

  // Callback for timeout event started when input event signals suspend.
  SIGNAL_CALLBACK_PACKED_1(PowerManDaemon, gboolean, RetrySuspend,
                           unsigned int);

  // Register the dbus message handler with appropriate dbus events.
  void RegisterDBusMessageHandler();

  // Generate UMA metrics on lid opening.
  void GenerateMetricsOnResumeEvent();

  // TODO(tbroch) refactor common methods for metrics
  // Sends a regular (exponential) histogram sample to Chrome for
  // transport to UMA. Returns true on success. See
  // MetricsLibrary::SendToUMA in metrics/metrics_library.h for a
  // description of the arguments.
  bool SendMetric(const std::string& name, int sample,
                  int min, int max, int nbuckets);

  // Emits a D-Bus signal announcing that the power or lock button has been
  // pressed or released.  |button_name| should be kPowerButtonName or
  // kLockButtonName from service_constants.h.
  void SendButtonEventSignal(const std::string& button_name,
                             ButtonState state);

  // Restart, shutdown, and suspend the system.
  void Restart();
  // The |reason| parameter is passed as the SHUTDOWN_REASON parameter to
  // initctl.
  void Shutdown(const std::string& reason);
  // Suspend(unsigned int, bool) is the real function. The other three redirect
  void Suspend(unsigned int wakeup_count, bool wakeup_count_valid);
  void Suspend();  // call suspend ignoring wakeup_count
  void Suspend(unsigned int wakeup_count);  // pass in explicit wakeup_count
  void Suspend(DBusMessage* message);  // get wakeup_count value from dbus

  // Lock and unlock virtual terminal switching.
  void LockVTSwitch();
  void UnlockVTSwitch();

  // Disable and Enable touch devices on lid close/open
  void DisableTouchDevices();
  void EnableTouchDevices(bool display_on);

  // Acquire the console file handle.
  bool GetConsole();

  GMainLoop* loop_;
  Input input_;
  bool use_input_for_lid_;
  bool use_input_for_key_power_;
  PowerPrefs* prefs_;
  LidState lidstate_;
  MetricsLibraryInterface* metrics_lib_;
  BacklightInterface* backlight_;
  int64 retry_suspend_ms_;
  int64 retry_suspend_attempts_;
  int retry_suspend_count_;
  pid_t suspend_pid_;
  unsigned int lid_id_;                // incremented on lid event
  unsigned int powerd_id_;             // incremented when powerd spawns/dies
  SessionManagerState session_state_;  // started | stopping | stopped
  PowerManagerState powerd_state_;     // alive | dead | unknown
  FilePath run_dir_;                   // --run_dir /var/run/power_manager
  FilePath lid_open_file_;             // touch when suspend should be cancelled
  base::TimeTicks lid_ticks_;          // log time for every lid event
  int console_fd_;

  // These are lookup tables that map dbus message interface/names to handlers.
  DBusSignalHandlerTable dbus_signal_handler_table_;
  DBusMethodHandlerTable dbus_method_handler_table_;
};

}  // namespace power_manager

#endif  // POWER_MANAGER_POWERMAN_H_
