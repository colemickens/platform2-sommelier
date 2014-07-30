// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/policy/suspend_delay_controller.h"

#include <algorithm>
#include <vector>

#include <base/bind.h>
#include <base/logging.h>
#include <base/message_loop/message_loop.h>
#include <base/strings/string_number_conversions.h>
#include <chromeos/dbus/service_constants.h>

#include "power_manager/common/util.h"
#include "power_manager/powerd/policy/suspend_delay_observer.h"
#include "power_manager/proto_bindings/suspend.pb.h"

namespace power_manager {
namespace policy {

namespace {

// Maximum time that the controller will wait for a suspend delay to become
// ready, in milliseconds.
const int kMaxDelayTimeoutMs = 10000;

}  // namespace

SuspendDelayController::SuspendDelayController(int initial_delay_id)
    : next_delay_id_(initial_delay_id),
      current_suspend_id_(0) {
}

SuspendDelayController::~SuspendDelayController() {
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
  LOG(INFO) << "Registering suspend delay " << delay_id << " ("
            << info.description << ")" << " of "
            << info.timeout.InMilliseconds() << " ms on behalf of "
            << dbus_client;
  registered_delays_.insert(std::make_pair(delay_id, info));

  reply->Clear();
  reply->set_delay_id(delay_id);
}

void SuspendDelayController::UnregisterSuspendDelay(
    const UnregisterSuspendDelayRequest& request,
    const std::string& dbus_client) {
  LOG(INFO) << "Unregistering suspend delay " << request.delay_id()
            << " (" << GetDelayDescription(request.delay_id()) << ")"
            << " on behalf of " << dbus_client;
  UnregisterDelayInternal(request.delay_id());
}

void SuspendDelayController::HandleSuspendReadiness(
    const SuspendReadinessInfo& info,
    const std::string& dbus_client) {
  int delay_id = info.delay_id();
  int suspend_id = info.suspend_id();
  LOG(INFO) << "Got notification that delay " << delay_id
            << " (" << GetDelayDescription(delay_id) << ") is ready for "
            << "suspend request " << suspend_id << " from " << dbus_client;

  if (suspend_id != current_suspend_id_) {
    LOG(WARNING) << "Ignoring readiness notification for wrong suspend request "
                 << "(got " << suspend_id << ", currently on "
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
              << " (" << GetDelayDescription(*it) << ") due to D-Bus client "
              << client << " going away";
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

  LOG(INFO) << "Announcing suspend request " << current_suspend_id_
            << " with " << delay_ids_being_waited_on_.size()
            << " pending delay(s) and " << old_count
            << " outstanding delay(s) from previous request";
  if (delay_ids_being_waited_on_.empty()) {
    PostNotifyObserversTask(current_suspend_id_);
  } else {
    base::TimeDelta max_timeout;
    for (DelayInfoMap::const_iterator it = registered_delays_.begin();
         it != registered_delays_.end(); ++it) {
      max_timeout = std::max(max_timeout, it->second.timeout);
    }
    max_timeout = std::min(
        max_timeout, base::TimeDelta::FromMilliseconds(kMaxDelayTimeoutMs));
    delay_expiration_timer_.Start(FROM_HERE, max_timeout, this,
        &SuspendDelayController::OnDelayExpiration);
  }
}

std::string SuspendDelayController::GetDelayDescription(int delay_id) const {
  DelayInfoMap::const_iterator it = registered_delays_.find(delay_id);
  return it != registered_delays_.end() ? it->second.description : "unknown";
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
    delay_expiration_timer_.Stop();
    PostNotifyObserversTask(current_suspend_id_);
  }
}

void SuspendDelayController::OnDelayExpiration() {
  std::string tardy_delays;
  for (std::set<int>::const_iterator it = delay_ids_being_waited_on_.begin();
       it != delay_ids_being_waited_on_.end(); ++it) {
    const DelayInfo& delay = registered_delays_[*it];
    if (!tardy_delays.empty())
      tardy_delays += ", ";
    tardy_delays += base::IntToString(*it) + " (" + delay.dbus_client + ": " +
        delay.description + ")";
  }
  LOG(WARNING) << "Timed out while waiting for suspend readiness "
               << "confirmation(s) for " << delay_ids_being_waited_on_.size()
               << " delay(s): " << tardy_delays;

  delay_ids_being_waited_on_.clear();
  PostNotifyObserversTask(current_suspend_id_);
}

void SuspendDelayController::PostNotifyObserversTask(int suspend_id) {
  notify_observers_timer_.Start(FROM_HERE, base::TimeDelta(),
      base::Bind(&SuspendDelayController::NotifyObservers,
                 base::Unretained(this), suspend_id));
}

void SuspendDelayController::NotifyObservers(int suspend_id) {
  LOG(INFO) << "Notifying observers that suspend is ready";
  FOR_EACH_OBSERVER(SuspendDelayObserver, observers_,
                    OnReadyForSuspend(this, suspend_id));
}

}  // namespace policy
}  // namespace power_manager
