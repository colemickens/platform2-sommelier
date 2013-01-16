// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SUSPENDER_H_
#define POWER_MANAGER_POWERD_SUSPENDER_H_

#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>

#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "power_manager/common/signal_callback.h"
#include "power_manager/powerd/screen_locker.h"
#include "power_manager/powerd/suspend_delay_observer.h"
#include "power_manager/suspend.pb.h"

namespace power_manager {

class Daemon;
class DBusSenderInterface;
class FileTagger;
class PowerPrefs;
class SuspendDelayController;

namespace system {
class Input;
}  // namespace system

class Suspender : public SuspendDelayObserver {
 public:
  Suspender(Daemon* daemon,
            ScreenLocker* locker,
            FileTagger* file_tagger,
            DBusSenderInterface* dbus_sender,
            system::Input* input,
            const FilePath& run_dir);
  ~Suspender();

  static void NameOwnerChangedHandler(DBusGProxy* proxy,
                                      const gchar* name,
                                      const gchar* old_owner,
                                      const gchar* new_owner,
                                      gpointer data);

  void Init(PowerPrefs* prefs);

  // Starts the suspend process.  Notifies clients that have registered delays
  // that the system is about to suspend, starts |check_suspend_timeout_id_|,
  // and requests that the screen be locked if needed.  Note that suspending
  // happens asynchronously.
  void RequestSuspend(bool cancel_if_lid_open);

  // Calls Suspend() if |suspend_requested_| is true and if it's now safe to do
  // so (i.e. there are no outstanding delays and the screen is locked if
  // |wait_for_screen_lock_| is set).
  void SuspendIfReady();

  // Cancels an outstanding suspend request.
  void CancelSuspend();

  // Handles a RegisterSuspendDelay call and returns a reply that should be sent
  // (or NULL if an empty reply should be sent).
  DBusMessage* RegisterSuspendDelay(DBusMessage* message);

  // Handles an UnregisterSuspendDelay call and returns a reply that should be
  // sent (or NULL if an empty reply should be sent).
  DBusMessage* UnregisterSuspendDelay(DBusMessage* message);

  // Handles a HandleSuspendReadiness call and returns a reply that should be
  // sent (or NULL if an empty reply should be sent).
  DBusMessage* HandleSuspendReadiness(DBusMessage* message);

  // Handle SuspendReady D-Bus signals.
  bool SuspendReady(DBusMessage* message);

  // Handles a PowerStateChanged signal emitted by the powerd_suspend script.
  void HandlePowerStateChanged(const char* state, int power_rc);

  // SuspendDelayObserver override:
  virtual void OnReadyForSuspend(int suspend_id) OVERRIDE;

 private:
  // Suspends the computer. Before this method is called, the system should be
  // in a state where it's truly ready to suspend (e.g. no outstanding delays,
  // screen locked if needed, etc.).
  void Suspend();

  // Callback for |retry_suspend_timeout_id_|.
  SIGNAL_CALLBACK_0(Suspender, gboolean, RetrySuspend);

  // Emits a D-Bus signal informing other processes that we've suspended or
  // resumed at |wall_time|.
  void SendSuspendStateChangedSignal(SuspendState_Type type,
                                     const base::Time& wall_time);

  // Callback for |screen_lock_timeout_id_|.  Invoked if screen lock is
  // taking too long; sets |wait_for_screen_lock_| to false and calls
  // SuspendIfReady().
  SIGNAL_CALLBACK_0(Suspender, gboolean, HandleScreenLockTimeout);

  // Clean up suspend delay upon unregister or dbus name change.
  // Remove |client_name| from list of suspend delay callback clients.
  bool CleanUpSuspendDelay(const char* client_name);

  Daemon* daemon_;
  ScreenLocker* locker_;
  FileTagger* file_tagger_;
  DBusSenderInterface* dbus_sender_;
  system::Input* input_;

  scoped_ptr<SuspendDelayController> suspend_delay_controller_;

  // Whether the computer will be suspended soon.
  bool suspend_requested_;

  // Unique ID associated with the current suspend attempt.
  int suspend_id_;

  // Should the suspend be canceled if the lid is opened midway through the
  // process?  This is generally true if the lid was already closed when
  // RequestSuspend() was called.
  bool cancel_suspend_if_lid_open_;

  // True if SuspendIfReady() should wait for |locker_| to report that the
  // screen is locked before suspending.  HandleScreenLockTimeout() sets
  // this back to false once the timeout has been hit.
  bool wait_for_screen_lock_;

  // ID of GLib source that will run HandleScreenLockTimeout(), or 0 if unset.
  guint screen_lock_timeout_id_;

  // Identify user activity to cancel suspend in progress.
  FilePath user_active_file_;

  // Number of wake events received at start of current suspend operation.
  unsigned int wakeup_count_;
  bool wakeup_count_valid_;

  // Time to wait before retrying a failed suspend attempt.
  base::TimeDelta retry_delay_;

  // Maximum number of times to retry a failed suspend attempt before giving up
  // and shutting down the system.
  int64 max_retries_;

  // Number of failed retries since RequestSuspend() was called.
  int num_retries_;

  // ID of GLib timeout that will run RetrySuspend() or 0 if unset.
  guint retry_suspend_timeout_id_;

  // Time at which the powerd_suspend script was last invoked to suspend the
  // system.  We cache this so it can be passed to
  // SendSuspendStateChangedSignal(): it's possible that the system will go to
  // sleep before HandlePowerStateChangedSignal() gets called in response to the
  // D-Bus signal that powerd_suspend emits before suspending, so we can't just
  // get the current time from there -- it may actually run post-resuming.  This
  // is a base::Time rather than base::TimeTicks since the monotonic clock
  // doesn't increase while we're suspended.
  base::Time last_suspend_wall_time_;

  DISALLOW_COPY_AND_ASSIGN(Suspender);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SUSPENDER_H_
