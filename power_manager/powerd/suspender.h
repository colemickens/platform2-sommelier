// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SUSPENDER_H_
#define POWER_MANAGER_POWERD_SUSPENDER_H_

#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "power_manager/common/signal_callback.h"
#include "power_manager/powerd/suspend_delay_observer.h"
#include "power_manager/suspend.pb.h"

namespace power_manager {

class Daemon;
class DBusSenderInterface;
class PrefsInterface;
class SuspendDelayController;

namespace system {
class Input;
}  // namespace system

// Suspender is responsible for suspending the system.  The typical flow is
// as follows:
//
// - RequestSuspend() is called when suspending is desired.
// - SuspendDelayController announces the new suspend request to processes that
//   have previously registered suspend delays via RegisterSuspendDelay().
// - OnReadyForSuspend() is called to announce that all processes have announced
//   readiness via HandleSuspendReadiness().  It calls Suspend(), which runs the
//   powerd_suspend script to begin the actual suspend process.
// - powerd_suspend emits a PowerStateChanged D-Bus signal with a "mem" argument
//   before asking the kernel to suspend and a second signal with an "on"
//   argument after the system resumes.
// - Suspender listens for PowerStateChanged and emits SuspendStateChanged
//   signals with additional details.  If the PowerStateChanged "on" signal
//   reported that the suspend attempt was unsuccessful, a timer is kept alive
//   to retry the suspend attempt.
//
// At any point during the suspend process, user activity can cancel the current
// suspend attempt.  If the powerd_suspend script has already been started, a
// file is touched to tell it to abort.
class Suspender : public SuspendDelayObserver {
 public:
  // Interface for classes responsible for performing actions on behalf of
  // Suspender.
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Is the lid currently closed?  Returns false if the query fails or if
    // the system doesn't have a lid.
    virtual bool IsLidClosed() = 0;

    // Reads the current wakeup count from sysfs and stores it in
    // |wakeup_count|.  Returns true on success.
    virtual bool GetWakeupCount(uint64* wakeup_count) = 0;

    // Synchronously runs the powerd_suspend script to suspend the system.
    // If |wakeup_count_valid| is true, passes |wakeup_count| to the script
    // so it can avoid suspending if additional wakeup events occur.
    virtual bool Suspend(uint64 wakeup_count, bool wakeup_count_valid) = 0;

    // Emits a PowerStateChanged D-Bus signal with an "on" status, similar to
    // what is emitted by powerd_suspend after resume.  Emitting this from
    // powerd is necessary when an imminent suspend has been announced but the
    // request is canceled before powerd_suspend has been run, so that processes
    // that have performed pre-suspend actions will know to undo them.
    virtual void EmitPowerStateChangedOnSignal() = 0;

    // Handles the system resuming (or recovering from a failed suspend
    // attempt).
    virtual void HandleResume(bool suspend_was_successful,
                              int num_suspend_retries,
                              int max_suspend_retries) = 0;

    // Shuts the system down in response to repeated failed suspend attempts.
    virtual void ShutdownForFailedSuspend() = 0;
  };

  // Helper class providing functionality needed by tests.
  class TestApi {
   public:
    explicit TestApi(Suspender* suspender);

    int suspend_id() const { return suspender_->suspend_id_; }

    // Sets the time returned by Suspender::GetCurrentTime().
    void SetCurrentWallTime(base::Time wall_time);

    // Runs Suspender::RetrySuspend() if |retry_suspend_timeout_id_| is set.
    // Returns false if the timeout wasn't set.
    bool TriggerRetryTimeout();

   private:
    Suspender* suspender_;  // not owned

    DISALLOW_COPY_AND_ASSIGN(TestApi);
  };

  // Creates a new delegate.  Ownership is passed to the caller.
  static Delegate* CreateDefaultDelegate(Daemon* daemon,
                                         system::Input* input);

  static void NameOwnerChangedHandler(DBusGProxy* proxy,
                                      const gchar* name,
                                      const gchar* old_owner,
                                      const gchar* new_owner,
                                      gpointer data);

  Suspender(Delegate* delegate, DBusSenderInterface* dbus_sender);
  ~Suspender();

  void Init(PrefsInterface* prefs);

  // Starts the suspend process.  Notifies clients that have registered
  // delays that the system is about to suspend.  Note that suspending
  // happens asynchronously.
  void RequestSuspend();

  // Handles a RegisterSuspendDelay call and returns a reply that should be sent
  // (or NULL if an empty reply should be sent).
  DBusMessage* RegisterSuspendDelay(DBusMessage* message);

  // Handles an UnregisterSuspendDelay call and returns a reply that should be
  // sent (or NULL if an empty reply should be sent).
  DBusMessage* UnregisterSuspendDelay(DBusMessage* message);

  // Handles a HandleSuspendReadiness call and returns a reply that should be
  // sent (or NULL if an empty reply should be sent).
  DBusMessage* HandleSuspendReadiness(DBusMessage* message);

  // Handles the lid being opened, user activity, or the system shutting down,
  // any of which may abort an in-progress suspend attempt.
  void HandleLidOpened();
  void HandleUserActivity();
  void HandleShutdown();

  // SuspendDelayObserver override:
  virtual void OnReadyForSuspend(int suspend_id) OVERRIDE;

 private:
  class RealDelegate;

  // Returns the current wall time or |current_wall_time_for_testing_| if set.
  base::Time GetCurrentWallTime() const;

  // Suspends the computer. Before this method is called, the system should be
  // in a state where it's truly ready to suspend (i.e. no outstanding delays).
  void Suspend();

  // Callback for |retry_suspend_timeout_id_|.
  SIGNAL_CALLBACK_0(Suspender, gboolean, RetrySuspend);

  // Cancels an outstanding suspend request.
  void CancelSuspend();

  // Emits a D-Bus signal informing other processes that we've suspended or
  // resumed at |wall_time|.
  void SendSuspendStateChangedSignal(SuspendState_Type type,
                                     const base::Time& wall_time);

  Delegate* delegate_;  // not owned
  DBusSenderInterface* dbus_sender_;  // not owned

  scoped_ptr<SuspendDelayController> suspend_delay_controller_;

  // Whether the system will be suspended soon.  This is set to true by
  // RequestSuspend() and set to false by OnReadyForSuspend().
  bool waiting_for_readiness_;

  // Unique ID associated with the current suspend request.
  int suspend_id_;

  // Number of wakeup events received at start of current suspend operation.
  uint64 wakeup_count_;
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

  // If non-empty, used in place of base::Time::Now() whenever the current
  // time is needed.
  base::Time current_wall_time_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(Suspender);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SUSPENDER_H_
