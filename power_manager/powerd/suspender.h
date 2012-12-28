// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SUSPENDER_H_
#define POWER_MANAGER_POWERD_SUSPENDER_H_

#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <map>
#include <string>

#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "power_manager/common/signal_callback.h"
#include "power_manager/powerd/screen_locker.h"
#include "power_manager/powerd/suspend_delay_observer.h"

namespace power_manager {

class Daemon;
class DBusSenderInterface;
class FileTagger;
class SuspendDelayController;

class Suspender : public SuspendDelayObserver {
 public:
  Suspender(ScreenLocker* locker,
            FileTagger* file_tagger,
            DBusSenderInterface* dbus_sender);
  ~Suspender();

  static void NameOwnerChangedHandler(DBusGProxy* proxy,
                                      const gchar* name,
                                      const gchar* old_owner,
                                      const gchar* new_owner,
                                      gpointer data);

  void Init(const FilePath& run_dir, Daemon* daemon);

  // Starts the suspend process.  Notifies clients that have registered delays
  // that the system is about to suspend, starts |check_suspend_timeout_id_|,
  // and requests that the screen be locked if needed.  Note that suspending
  // happens asynchronously.
  void RequestSuspend();

  // Calls Suspend() if |suspend_requested_| is true and if it's now safe to do
  // so (i.e. there are no outstanding delays and the screen is locked if
  // |wait_for_screen_lock_| is set).
  void CheckSuspend();

  // Cancels an outstanding suspend request.
  void CancelSuspend();

  // Handles a RegisterSuspendDelay call and returns a reply that should be sent
  // (or NULL if an empty reply should be sent).
  DBusMessage* RegisterSuspendDelay(DBusMessage* message);

  // Handles an UnregisterSuspendDelay call and returns a reply that should be
  // sent (or NULL if an empty reply should be sent).
  DBusMessage* UnregisterSuspendDelay(DBusMessage* message);

  // Handles HandleSuspendReadiness call and returns a reply that should be sent
  // (or NULL if an empty reply should be sent).
  DBusMessage* HandleSuspendReadiness(DBusMessage* message);

  // Handle SuspendReady D-Bus signals.
  bool SuspendReady(DBusMessage* message);

  // SuspendDelayObserver override:
  virtual void OnReadyForSuspend(int suspend_id) OVERRIDE;

 private:
  // Suspends the computer. Before this method is called, the system should be
  // in a state where it's truly ready to suspend (e.g. no outstanding delays,
  // screen locked if needed, etc.).
  void Suspend();

  // Timeout callback in case suspend clients do not respond in time.
  // Always returns false.
  SIGNAL_CALLBACK_0(Suspender, gboolean, CheckSuspendTimeout);

  // Clean up suspend delay upon unregister or dbus name change.
  // Remove |client_name| from list of suspend delay callback clients.
  bool CleanUpSuspendDelay(const char* client_name);

  void BroadcastSignalToClients(const char* signal_name, int sequence_num);

  ScreenLocker* locker_;
  FileTagger* file_tagger_;

  scoped_ptr<SuspendDelayController> suspend_delay_controller_;

  // Whether the computer should be suspended soon.
  unsigned int suspend_delay_timeout_ms_;
  int suspend_delays_outstanding_;
  bool suspend_requested_;
  int suspend_sequence_number_;

  // ID of GLib source that will run CheckSuspendTimeout(), or 0 if unset.
  unsigned int check_suspend_timeout_id_;

  // True if CheckSuspend() should wait for |locker_| to report that the screen
  // is locked before suspending.  CheckSuspendTimeout() sets this back to false
  // once the timeout has been hit.
  bool wait_for_screen_lock_;

  // Identify user activity to cancel suspend in progress.
  FilePath user_active_file_;

  // suspend_delays_ : map from dbus unique identifiers to expected ms delays.
  typedef std::map<std::string, uint32> SuspendList;
  SuspendList suspend_delays_;

  // Number of wake events received at start of current suspend operation
  unsigned int wakeup_count_;
  bool wakeup_count_valid_;

  // Reference to the owning daemon, so that this class can callback to it
  Daemon* daemon_;

  DISALLOW_COPY_AND_ASSIGN(Suspender);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SUSPENDER_H_
