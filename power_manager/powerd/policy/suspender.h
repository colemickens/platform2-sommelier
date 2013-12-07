// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_POLICY_SUSPENDER_H_
#define POWER_MANAGER_POWERD_POLICY_SUSPENDER_H_

#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "base/timer.h"
#include "power_manager/powerd/policy/suspend_delay_observer.h"
#include "power_manager/suspend.pb.h"

namespace power_manager {

class Clock;
class DBusSenderInterface;
class PrefsInterface;

namespace system {
class Input;
}  // namespace system

namespace policy {

class DarkResumePolicy;
class SuspendDelayController;

// Suspender is responsible for suspending the system.  The typical flow is
// as follows:
//
// - RequestSuspend() is called when suspending is desired.
// - SuspendDelayController announces the new suspend request to processes that
//   have previously registered suspend delays via RegisterSuspendDelay().
// - OnReadyForSuspend() is called to announce that all processes have announced
//   readiness via HandleSuspendReadiness().  It calls Suspend(), which runs the
//   powerd_suspend script to perform the actual suspend/resume cycle.
// - powerd_suspend emits a PowerStateChanged D-Bus signal with a "mem" argument
//   before asking the kernel to suspend and a second signal with an "on"
//   argument after the system resumes.
// - If powerd_suspend reports failure, a timeout is created to retry the
//   suspend attempt.
//
// At any point before Suspend() has been called, user activity can cancel
// the current suspend attempt.  A synthetic PowerStateChanged "on" signal
// is emitted so that other processes can undo any setup that they did in
// response to suspend delays.
class Suspender : public SuspendDelayObserver {
 public:
  // Interface for classes responsible for performing actions on behalf of
  // Suspender.  The general sequence when suspending is:
  //
  // - Suspender::RequestSuspend() calls PrepareForSuspendAnnouncement()
  //   and then notifies other processes that the system is about to
  //   suspend.
  // - If the suspend attempt is canceled while Suspender is still waiting
  //   for other processes to report readiness for suspend, then
  //   HandleCanceledSuspendAnnouncementIsCalled().
  // - Otherwise, PrepareForSuspend() is called.
  // - Suspend() is then called within a do while loop that exits when there is
  //   a user requested suspend or enough failures.
  // - After the system resumes from suspend (or if the suspend attempt
  //   failed), HandleResume() is called.
  class Delegate {
   public:
    // Outcomes for a suspend attempt.
    enum SuspendResult {
      // The system successfully suspended and resumed.
      SUSPEND_SUCCESSFUL = 0,
      // The kernel reported a (possibly transient) error while suspending.
      SUSPEND_FAILED,
      // The suspend attempt was canceled as a result of a wakeup event.
      SUSPEND_CANCELED,
    };

    virtual ~Delegate() {}

    // Is the lid currently closed?  Returns false if the query fails or if
    // the system doesn't have a lid.
    virtual bool IsLidClosed() = 0;

    // Reads the current wakeup count from sysfs and stores it in
    // |wakeup_count|.  Returns true on success.
    virtual bool GetWakeupCount(uint64* wakeup_count) = 0;

    // Performs any work that needs to happen before other processes are
    // informed that the system is about to suspend.  Called by
    // RequestSuspend().
    virtual void PrepareForSuspendAnnouncement() = 0;

    // Called if the suspend request is aborted before Suspend() and
    // HandleResume() are called.  This method should undo any work done by
    // PrepareForSuspendAnnouncement().
    virtual void HandleCanceledSuspendAnnouncement() = 0;

    // Handles putting the system into the correct state before suspending such
    // as suspending the backlight, audio, and sending out the PowerStateChanged
    // signal. This is separate from Suspend(...) since for the purpose of a
    // dark resume (which can call suspend again), we don't want to touch the
    // state of the backlight or audio. We also don't want to send out another
    // PowerStateChanged signal since those are not sent out during a dark
    // resume.
    virtual void PrepareForSuspend() = 0;

    // Synchronously runs the powerd_suspend script to suspend the system.
    // If |wakeup_count_valid| is true, passes |wakeup_count| to the script
    // so it can avoid suspending if additional wakeup events occur.  After
    // the suspend/resume cycle is complete (and even if the system failed
    // to suspend), HandleResume() will be called.
    virtual SuspendResult Suspend(uint64 wakeup_count,
                                  bool wakeup_count_valid,
                                  base::TimeDelta duration) = 0;

    // Handles the system resuming (or recovering from a failed suspend
    // attempt).  This method should undo any work done by both
    // PrepareForSuspendAnnouncement() and PrepareForSuspend().
    virtual void HandleResume(bool suspend_was_successful,
                              int num_suspend_retries,
                              int max_suspend_retries) = 0;

    // Shuts the system down in response to repeated failed suspend attempts.
    virtual void ShutDownForFailedSuspend() = 0;

    // Shuts the system down in response to the DarkResumePolicy determining the
    // system should shutdown.
    virtual void ShutDownForDarkResume() = 0;
  };

  // Helper class providing functionality needed by tests.
  class TestApi {
   public:
    explicit TestApi(Suspender* suspender);

    int suspend_id() const { return suspender_->suspend_id_; }

    // Sets the time used as "now".
    void SetCurrentWallTime(base::Time wall_time);

    // Runs Suspender::RetrySuspend() if |retry_suspend_timer_| is running.
    // Returns false otherwise.
    bool TriggerRetryTimeout();

   private:
    Suspender* suspender_;  // not owned

    DISALLOW_COPY_AND_ASSIGN(TestApi);
  };

  Suspender();
  virtual ~Suspender();

  void Init(Delegate* delegate,
            DBusSenderInterface* dbus_sender,
            DarkResumePolicy* dark_resume_policy,
            PrefsInterface* prefs);

  // Starts the suspend process. Note that suspending happens
  // asynchronously.
  void RequestSuspend();

  // Like RequestSuspend(), but aborts the suspend attempt immediately if
  // the current wakeup count reported by the kernel exceeds
  // |wakeup_count|. Autotests can pass an external wakeup count to ensure
  // that machines in the test cluster don't sleep indefinitely (see
  // http://crbug.com/218175).
  void RequestSuspendWithExternalWakeupCount(uint64 wakeup_count);

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

  // Handles the D-Bus name |name| becoming owned by |new_owner| instead of
  // |old_owner|.
  void HandleDBusNameOwnerChanged(const std::string& name,
                                  const std::string& old_owner,
                                  const std::string& new_owner);

  // SuspendDelayObserver override:
  virtual void OnReadyForSuspend(int suspend_id) OVERRIDE;

 private:
  // Notifies clients that have registered delays that the system is about
  // to suspend. Internal implementation shared by RequestSuspend() and
  // RequestSuspendWithExternalWakeupCount().
  void StartSuspendAttempt();

  // Actually suspends the system. Before this method is called, the system
  // should be in a state where it's truly ready to suspend (i.e. no
  // outstanding delays).
  void Suspend();

  // Retries a suspend attempt.
  void RetrySuspend();

  // Cancels an outstanding suspend request.
  void CancelSuspend();

  // Emits a D-Bus signal informing other processes that we've suspended or
  // resumed at |wall_time|.
  void SendSuspendStateChangedSignal(SuspendState_Type type,
                                     const base::Time& wall_time);

  Delegate* delegate_;  // not owned
  DBusSenderInterface* dbus_sender_;  // not owned
  DarkResumePolicy* dark_resume_policy_;  // not owned

  scoped_ptr<Clock> clock_;
  scoped_ptr<SuspendDelayController> suspend_delay_controller_;

  // Whether the system will be suspended soon.  This is set to true by
  // RequestSuspend() and set to false by OnReadyForSuspend().
  bool waiting_for_readiness_;

  // Unique ID associated with the current suspend request.
  int suspend_id_;

  // Number of wakeup events received at start of current suspend operation.
  uint64 wakeup_count_;
  bool wakeup_count_valid_;

  // Did |wakeup_count_| come from an external process via
  // RequestSuspendWithExternalWakeupCount()?
  bool got_external_wakeup_count_;

  // The duration the machine should suspend for.
  int64 suspend_duration_;

  // Time to wait before retrying a failed suspend attempt.
  base::TimeDelta retry_delay_;

  // Maximum number of times to retry a failed suspend attempt before giving up
  // and shutting down the system.
  int64 max_retries_;

  // Number of failed retries since RequestSuspend() was called.
  int num_retries_;

  // Runs RetrySuspend().
  base::OneShotTimer<Suspender> retry_suspend_timer_;

  // True after this class has received notification that the system is
  // shutting down.
  bool shutting_down_;

  DISALLOW_COPY_AND_ASSIGN(Suspender);
};

}  // namespace policy
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_POLICY_SUSPENDER_H_
