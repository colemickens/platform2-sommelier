// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_POLICY_SUSPEND_DELAY_CONTROLLER_H_
#define POWER_MANAGER_POWERD_POLICY_SUSPEND_DELAY_CONTROLLER_H_
#pragma once

#include <map>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/observer_list.h"
#include "base/time.h"
#include "base/timer.h"

namespace power_manager {

class DBusSenderInterface;
class RegisterSuspendDelayReply;
class RegisterSuspendDelayRequest;
class SuspendReadinessInfo;
class UnregisterSuspendDelayRequest;

namespace policy {

class SuspendDelayObserver;

// Handles D-Bus requests to delay suspending until other processes have had
// time to do last-minute cleanup.
class SuspendDelayController {
 public:
  explicit SuspendDelayController(DBusSenderInterface* dbus_sender);
  ~SuspendDelayController();

  bool ready_for_suspend() const { return delay_ids_being_waited_on_.empty(); }

  // Adds or removes an observer that will be notified when it's safe to
  // suspend.
  void AddObserver(SuspendDelayObserver* observer);
  void RemoveObserver(SuspendDelayObserver* observer);

  // Registers a new suspend delay on behalf of |dbus_client| and fills |reply|
  // with the message that should be returned.
  void RegisterSuspendDelay(const RegisterSuspendDelayRequest& request,
                            const std::string& dbus_client,
                            RegisterSuspendDelayReply* reply);

  // Unregisters a previously-registered suspend delay.
  void UnregisterSuspendDelay(const UnregisterSuspendDelayRequest& request,
                              const std::string& dbus_client);

  // Handles notification that a client has reported readiness for suspend.
  void HandleSuspendReadiness(const SuspendReadinessInfo& info,
                              const std::string& dbus_client);

  // Called when |client| has gone away (i.e. a NameOwnerChanged signal was
  // received with |client| in its |name| field and an empty |new_owner| field.
  void HandleDBusClientDisconnected(const std::string& client);

  // Called when suspend is desired.  Updates |current_suspend_id_| and
  // |delays_being_waited_on_| and notifies clients that suspend is imminent.
  void PrepareForSuspend(int suspend_id);

 private:
  // Information about a registered delay.
  struct DelayInfo {
    // Maximum amount of time to wait for HandleSuspendReadiness() to be called
    // after a suspend has been requested.
    base::TimeDelta timeout;

    // Name of the D-Bus connection that registered the delay.
    std::string dbus_client;

    // Human-readable description supplied with the registration request.
    std::string description;
  };

  // Removes |delay_id| from |registered_delays_| and calls
  // RemoveDelayFromWaitList().
  void UnregisterDelayInternal(int delay_id);

  // Removes |delay_id| from |delay_ids_being_waited_on_|.  If the set goes from
  // non-empty to empty, cancels the delay expiration timeout and notifies
  // observers that it's safe to to suspend.
  void RemoveDelayFromWaitList(int delay_id);

  // Called by |delay_expiration_timer_| after a PrepareForSuspend() call if
  // HandleSuspendReadiness() isn't invoked for all registered delays before the
  // maximum delay timeout has elapsed.  Notifies observers that it's safe to
  // suspend.
  void OnDelayExpiration();

  // Posts a NotifyObservers() call to the message loop.
  void PostNotifyObserversTask(int suspend_id);

  // Invokes OnReadyForSuspend() on |observers_|.
  void NotifyObservers(int suspend_id);

  // Used to emit D-Bus signals.
  DBusSenderInterface* dbus_sender_;

  // Map from delay ID to registered delay.
  typedef std::map<int, DelayInfo> DelayInfoMap;
  DelayInfoMap registered_delays_;

  // Next delay ID that will be returned in response to a call to
  // RegisterSuspendDelay().
  int next_delay_id_;

  // ID corresponding to the current (or most-recent) suspend attempt.
  int current_suspend_id_;

  // IDs of delays registered by clients that haven't yet said they're ready to
  // suspend.
  std::set<int> delay_ids_being_waited_on_;

  // Used to invoke NotifyObservers().
  base::OneShotTimer<SuspendDelayController> notify_observers_timer_;

  // Used to invoke OnDelayExpiration().
  base::OneShotTimer<SuspendDelayController> delay_expiration_timer_;

  ObserverList<SuspendDelayObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(SuspendDelayController);
};

}  // namespace policy
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_POLICY_SUSPEND_DELAY_CONTROLLER_H_
