// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/policy/suspend_delay_controller.h"

#include <glib.h>

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "chromeos/dbus/service_constants.h"
#include "power_manager/common/dbus_sender.h"
#include "power_manager/common/util.h"
#include "power_manager/powerd/policy/suspend_delay_observer.h"
#include "power_manager/suspend.pb.h"

namespace power_manager {
namespace policy {

namespace {

// Maximum time that the controller will wait for a suspend delay to become
// ready, in milliseconds.
const int kMaxDelayTimeoutMs = 10000;

}  // namespace

SuspendDelayController::SuspendDelayController(DBusSenderInterface* dbus_sender)
    : dbus_sender_(dbus_sender),
      next_delay_id_(1),
      current_suspend_id_(0),
      notify_observers_timeout_id_(0),
      delay_expiration_timeout_id_(0) {
  DCHECK(dbus_sender_);
}

SuspendDelayController::~SuspendDelayController() {
  util::RemoveTimeout(&notify_observers_timeout_id_);
  util::RemoveTimeout(&delay_expiration_timeout_id_);
  dbus_sender_ = NULL;
}

void SuspendDelayController::AddObserver(SuspendDelayObserver* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void SuspendDelayController::RemoveObserver(SuspendDelayObserver* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

void SuspendDelayController::RegisterSuspendDelay(
    const RegisterSuspendDelayRequest& request,
    const std::string& dbus_client,
    RegisterSuspendDelayReply* reply) {
  DCHECK(reply);

  int delay_id = next_delay_id_++;

  DelayInfo info;
  info.timeout = base::TimeDelta::FromInternalValue(request.timeout());
  info.dbus_client = dbus_client;
  info.description = request.description();
  LOG(INFO) << "Registering suspend delay " << delay_id
            << " of " << info.timeout.InMilliseconds() << " ms on behalf of "
            << dbus_client << " (" << info.description << ")";
  registered_delays_.insert(std::make_pair(delay_id, info));

  reply->Clear();
  reply->set_delay_id(delay_id);
}

void SuspendDelayController::UnregisterSuspendDelay(
    const UnregisterSuspendDelayRequest& request,
    const std::string& dbus_client) {
  LOG(INFO) << "Unregistering suspend delay " << request.delay_id()
            << " on behalf of " << dbus_client;
  UnregisterDelayInternal(request.delay_id());
}

void SuspendDelayController::HandleSuspendReadiness(
    const SuspendReadinessInfo& info,
    const std::string& dbus_client) {
  int delay_id = info.delay_id();
  int suspend_id = info.suspend_id();
  LOG(INFO) << "Got notification that client with delay " << delay_id
            << " is ready for suspend attempt " << suspend_id << " from "
            << dbus_client;

  if (suspend_id != current_suspend_id_) {
    LOG(WARNING) << "Ignoring readiness notification for wrong suspend attempt "
                 << "(got " << suspend_id << ", currently attempting "
                 << current_suspend_id_ << ")";
    return;
  }

  if (!delay_ids_being_waited_on_.count(delay_id)) {
    LOG(WARNING) << "Ignoring readiness notification for delay " << delay_id
                 << ", which we weren't waiting for";
    return;
  }
  RemoveDelayFromWaitList(delay_id);
}

void SuspendDelayController::HandleDBusClientDisconnected(
    const std::string& client) {
  std::vector<int> delay_ids_to_remove;
  for (DelayInfoMap::const_iterator it = registered_delays_.begin();
       it != registered_delays_.end(); ++it) {
    if (it->second.dbus_client == client)
      delay_ids_to_remove.push_back(it->first);
  }

  for (std::vector<int>::const_iterator it = delay_ids_to_remove.begin();
       it != delay_ids_to_remove.end(); ++it) {
    LOG(INFO) << "Unregistering suspend delay " << *it
              << " due to D-Bus client " << client << " going away";
    UnregisterDelayInternal(*it);
  }
}

void SuspendDelayController::PrepareForSuspend(int suspend_id) {
  current_suspend_id_ = suspend_id;

  size_t old_count = delay_ids_being_waited_on_.size();
  delay_ids_being_waited_on_.clear();
  for (DelayInfoMap::const_iterator it = registered_delays_.begin();
       it != registered_delays_.end(); ++it)
    delay_ids_being_waited_on_.insert(it->first);

  LOG(INFO) << "Announcing suspend attempt " << current_suspend_id_
            << " with " << delay_ids_being_waited_on_.size()
            << " pending delay(s) and " << old_count
            << " outstanding delay(s) from previous attempt";
  if (delay_ids_being_waited_on_.empty()) {
    PostNotifyObserversTask(current_suspend_id_);
  } else {
    util::RemoveTimeout(&delay_expiration_timeout_id_);
    base::TimeDelta max_timeout;
    for (DelayInfoMap::const_iterator it = registered_delays_.begin();
         it != registered_delays_.end(); ++it) {
      max_timeout = std::max(max_timeout, it->second.timeout);
    }
    max_timeout = std::min(
        max_timeout, base::TimeDelta::FromMilliseconds(kMaxDelayTimeoutMs));
    delay_expiration_timeout_id_ = g_timeout_add(
        max_timeout.InMilliseconds(), OnDelayExpirationThunk, this);
  }

  SuspendImminent proto;
  proto.set_suspend_id(current_suspend_id_);
  dbus_sender_->EmitSignalWithProtocolBuffer(kSuspendImminentSignal, proto);
}

void SuspendDelayController::UnregisterDelayInternal(int delay_id) {
  if (!registered_delays_.count(delay_id)) {
    LOG(WARNING) << "Ignoring request to remove unknown suspend delay "
                 << delay_id;
    return;
  }
  RemoveDelayFromWaitList(delay_id);
  registered_delays_.erase(delay_id);
}

void SuspendDelayController::RemoveDelayFromWaitList(int delay_id) {
  if (!delay_ids_being_waited_on_.count(delay_id))
    return;

  delay_ids_being_waited_on_.erase(delay_id);
  if (delay_ids_being_waited_on_.empty()) {
    util::RemoveTimeout(&delay_expiration_timeout_id_);
    PostNotifyObserversTask(current_suspend_id_);
  }
}

gboolean SuspendDelayController::OnDelayExpiration() {
  delay_expiration_timeout_id_ = 0;

  std::string tardy_delays;
  for (std::set<int>::const_iterator it = delay_ids_being_waited_on_.begin();
       it != delay_ids_being_waited_on_.end(); ++it) {
    const DelayInfo& delay = registered_delays_[*it];
    if (!tardy_delays.empty())
      tardy_delays += ", ";
    tardy_delays += base::IntToString(*it) + " (" + delay.dbus_client + ": " +
        delay.description + ")";
  }
  LOG(WARNING) << "Timed out while waiting for suspend readiness confirmations "
               << "for " << delay_ids_being_waited_on_.size() << " delay(s): "
               << tardy_delays;

  delay_ids_being_waited_on_.clear();
  PostNotifyObserversTask(current_suspend_id_);
  return FALSE;
}

void SuspendDelayController::PostNotifyObserversTask(int suspend_id) {
  util::RemoveTimeout(&notify_observers_timeout_id_);
  notify_observers_timeout_id_ = g_timeout_add(
      0, NotifyObserversThunk, CreateNotifyObserversArgs(this, suspend_id));
}

gboolean SuspendDelayController::NotifyObservers(int suspend_id) {
  LOG(INFO) << "Notifying observers that suspend is ready";
  notify_observers_timeout_id_ = 0;
  FOR_EACH_OBSERVER(SuspendDelayObserver, observers_,
                    OnReadyForSuspend(suspend_id));
  return FALSE;
}

}  // namespace policy
}  // namespace power_manager
