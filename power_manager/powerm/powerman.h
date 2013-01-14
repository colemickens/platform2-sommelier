// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERM_POWERMAN_H_
#define POWER_MANAGER_POWERM_POWERMAN_H_

#include <dbus/dbus-glib-lowlevel.h>
#include <sys/types.h>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "metrics/metrics_library.h"
#include "power_manager/common/power_constants.h"
#include "power_manager/common/power_prefs.h"
#include "power_manager/common/signal_callback.h"
#include "power_manager/common/util_dbus_handler.h"
#include "power_manager/suspend.pb.h"

namespace power_manager {

class DBusSenderInterface;

class PowerManDaemon {
 public:
  PowerManDaemon(PowerPrefs* prefs,
                 MetricsLibraryInterface* metrics_lib,
                 const FilePath& run_dir);
  virtual ~PowerManDaemon();

  void Init();
  void Run();

 private:
  friend class PowerManDaemonTest;
  FRIEND_TEST(PowerManDaemonTest, SendMetric);
  FRIEND_TEST(PowerManDaemonTest, GenerateRetrySuspendCountMetric);

  // UMA metrics parameters.
  static const char kMetricRetrySuspendCountName[];
  static const int kMetricRetrySuspendCountMin;
  static const int kMetricRetrySuspendCountBuckets;

  // Callbacks for handling dbus messages.
  bool HandleSuspendSignal(DBusMessage* message);
  bool HandleShutdownSignal(DBusMessage* message);
  bool HandleRestartSignal(DBusMessage* message);
  bool HandleRequestCleanShutdownSignal(DBusMessage* message);
  bool HandlePowerStateChangedSignal(DBusMessage* message);

  bool CancelDBusRequest();

  // Callback for timeout event started when input event signals suspend.
  SIGNAL_CALLBACK_0(PowerManDaemon, gboolean, RetrySuspend);

  // Register the dbus message handler with appropriate dbus events.
  void RegisterDBusMessageHandler();

  // Emits a D-Bus signal informing other processes that we've suspended or
  // resumed at |wall_time|.
  void SendSuspendStateChangedSignal(SuspendState_Type type,
                                     const base::Time& wall_time);

  // Generate UMA metrics on lid opening.
  void GenerateMetricsOnResumeEvent();

  // TODO(tbroch) refactor common methods for metrics
  // Sends a regular (exponential) histogram sample to Chrome for
  // transport to UMA. Returns true on success. See
  // MetricsLibrary::SendToUMA in metrics/metrics_library.h for a
  // description of the arguments.
  bool SendMetric(const std::string& name, int sample,
                  int min, int max, int nbuckets);

  // Restarts the system.
  void Restart();

  // Shuts the system down.  The |reason| parameter is passed as the
  // SHUTDOWN_REASON parameter to initctl.
  void Shutdown(const std::string& reason);

  // Suspends the system.  |wakeup_count| and |cancel_if_lid_open| are passed to
  // the powerd_suspend script.
  void Suspend(unsigned int wakeup_count,
               bool wakeup_count_valid,
               bool cancel_if_lid_open);

  // Lock and unlock virtual terminal switching.
  void LockVTSwitch();
  void UnlockVTSwitch();

  // Acquire the console file handle.
  bool GetConsole();

  GMainLoop* loop_;
  PowerPrefs* prefs_;
  MetricsLibraryInterface* metrics_lib_;
  int64 retry_suspend_ms_;
  int64 retry_suspend_attempts_;
  int retry_suspend_count_;
  pid_t suspend_pid_;
  FilePath run_dir_;                   // --run_dir /var/run/power_manager
  FilePath lid_open_file_;             // touch when suspend should be cancelled
  int console_fd_;

  // ID of GLib timeout that will run RetrySuspend() or 0 if unset.
  guint retry_suspend_timeout_id_;

  // Value of |cancel_if_lid_open| passed to Suspend().  Cached here so that
  // RetrySuspend() can invoke Suspend() with the same value as the original
  // request.
  bool cancel_suspend_if_lid_open_;

  // This is the DBus helper object that dispatches DBus messages to handlers
  util::DBusHandler dbus_handler_;

  scoped_ptr<DBusSenderInterface> dbus_sender_;

  // Time at which the powerd_suspend script was last invoked to suspend the
  // system.  We cache this so it can be passed to
  // SendSuspendStateChangedSignal(): it's possible that the system will go to
  // sleep before HandlePowerStateChangedSignal() gets called in response to the
  // D-Bus signal that powerd_suspend emits before suspending, so we can't just
  // get the current time from there -- it may actually run post-resuming.  This
  // is a base::Time rather than base::TimeTicks since the monotonic clock
  // doesn't increase while we're suspended.
  base::Time last_suspend_wall_time_;
};

}  // namespace power_manager

#endif  // POWER_MANAGER_POWERM_POWERMAN_H_
