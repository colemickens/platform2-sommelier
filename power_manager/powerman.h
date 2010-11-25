// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERMAN_H_
#define POWER_MANAGER_POWERMAN_H_

#include <sys/types.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <gdk/gdk.h>

#include "base/file_path.h"
#include "base/time.h"
#include "metrics/metrics_library.h"
#include "power_manager/input.h"
#include "power_manager/power_prefs.h"
#include "power_manager/powerd.h"

namespace power_manager {

class PowerManDaemon {
 public:
  PowerManDaemon(PowerPrefs* prefs, MetricsLibraryInterface* metrics_lib,
                 const FilePath& run_dir);
  virtual ~PowerManDaemon();

  void Init();
  void Run();

 private:
  friend class PowerManDaemonTest;
  FRIEND_TEST(PowerManDaemonTest, SendMetric);
  FRIEND_TEST(PowerManDaemonTest, GenerateRetrySuspendCountMetric);

  enum LidState { kLidClosed, kLidOpened };
  inline LidState GetLidState(int value) {
    // value == 0 is open. value == 1 is closed.
    return (value == 1) ? kLidClosed : kLidOpened;
  }

  enum PowerButtonState { kPowerButtonDown, kPowerButtonUp };
  inline PowerButtonState GetPowerButtonState(int value) {
    // value == 0 is button up. value == 1 is button down.
    // value == 2 is key repeat.
    return (value > 0) ? kPowerButtonDown : kPowerButtonUp;
  }

  // UMA metrics parameters.
  static const char kMetricRetrySuspendCountName[];
  static const int kMetricRetrySuspendCountMin;
  static const int kMetricRetrySuspendCountBuckets;

  // Handler for input events. |object| contains a pointer to a PowerManDaemon
  // object. |type| contains the event type (lid or power button). |value|
  // contains the new state of this input device.
  static void OnInputEvent(void* object, InputType type, int value);

  // Standard handler for dbus messages. |data| contains a pointer to a
  // PowerManDaemon object.
  static DBusHandlerResult DBusMessageHandler(DBusConnection*,
                                              DBusMessage* message,
                                              void* data);

  // Payload for RetrySuspend
  struct RetrySuspendPayload {
    unsigned int lid_id;
    unsigned int powerd_id;
    PowerManDaemon* daemon;
  };
  // allocate payload for suspend related checking
  RetrySuspendPayload* CreateRetrySuspendPayload();

  bool CancelDBusRequest();

  enum PowerManagerState { kPowerManagerUnknown, kPowerManagerAlive,
                           kPowerManagerDead };
  // Handler for NameOwnerChanged dbus messages.  See dbus-specification
  // at dbus.freedesktop.org for complete details of arguments
  static void DBusNameOwnerChangedHandler(
      DBusGProxy*, const gchar* name, const gchar* old_owner,
      const gchar* new_owner, void*);

  // Callback for timeout event started when lid closed to validate powerd has
  // received it successfully
  static gboolean CheckLidClosed(gpointer object);

  // Callback for timeout event started when input event signals suspend.
  // |object| contains a pointer to a PowerManDaemon object.
  static gboolean RetrySuspend(gpointer object);

  // Add interfaces to dbus matches for connection
  void AddDBusMatch(DBusConnection *connection, const char *interface);
  // Register the dbus message handler with appropriate dbus events.
  void RegisterDBusMessageHandler();

  // Generate UMA metrics on lid opening
  void GenerateMetricsOnResumeEvent();

  // TODO(tbroch) refactor common methods for metrics
  // Sends a regular (exponential) histogram sample to Chrome for
  // transport to UMA. Returns true on success. See
  // MetricsLibrary::SendToUMA in metrics/metrics_library.h for a
  // description of the arguments.
  bool SendMetric(const std::string& name, int sample,
                  int min, int max, int nbuckets);

  // Restart, shutdown, and suspend the system.
  void Restart();
  void Shutdown();
  void Suspend();

  // Lock and unlock virtual terminal switching.
  void LockVTSwitch();
  void UnlockVTSwitch();

  // Acquire the console file handle.
  bool GetConsole();

  GMainLoop* loop_;
  Input input_;
  bool use_input_for_lid_;
  bool use_input_for_key_power_;
  PowerPrefs* prefs_;
  LidState lidstate_;
  MetricsLibraryInterface* metrics_lib_;
  int power_button_state_;
  int64 retry_suspend_ms_;
  int64 retry_suspend_attempts_;
  int retry_suspend_count_;
  pid_t suspend_pid_;
  unsigned int lid_id_;              // incremented on lid event
  unsigned int powerd_id_;           // incremented when powerd spawns/dies
  PowerManagerState powerd_state_;   // alive | dead | unknown
  FilePath run_dir_;                 // --run_dir /var/run/power_manager
  FilePath lid_open_file_;           // touch when suspend should be cancelled
  base::TimeTicks lid_ticks_;        // log time for every lid event
  int console_fd_;
};

}  // namespace power_manager

#endif  // POWER_MANAGER_POWERMAN_H_
