// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/power_manager.h"

#include <map>
#include <string>

#include <base/stl_util.h>

#include "shill/logging.h"
#include "shill/power_manager_proxy_interface.h"
#include "shill/proxy_factory.h"

using std::map;
using std::string;

namespace shill {

PowerManager::PowerManager(ProxyFactory *proxy_factory)
    : power_manager_proxy_(proxy_factory->CreatePowerManagerProxy(this)),
      power_state_(kUnknown) {}

PowerManager::~PowerManager() {
}

void PowerManager::AddStateChangeCallback(const string &key,
                                          const PowerStateCallback &callback) {
  SLOG(Power, 2) << __func__ << " key " << key;
  AddCallback(key, callback, &state_change_callbacks_);
}

void PowerManager::AddSuspendDelayCallback(
    const string &key, const SuspendDelayCallback &callback) {
  SLOG(Power, 2) << __func__ << " key " << key;
  AddCallback(key, callback, &suspend_delay_callbacks_);
}

void PowerManager::RemoveStateChangeCallback(const string &key) {
  SLOG(Power, 2) << __func__ << " key " << key;
  RemoveCallback(key, &state_change_callbacks_);
}

void PowerManager::RemoveSuspendDelayCallback(const string &key) {
  SLOG(Power, 2) << __func__ << " key " << key;
  RemoveCallback(key, &suspend_delay_callbacks_);
}

void PowerManager::OnSuspendDelay(uint32 sequence_number) {
  SLOG(Power, 2) << __func__ << " sequence number " << sequence_number;
  OnEvent(sequence_number, &suspend_delay_callbacks_);
}

void PowerManager::OnPowerStateChanged(SuspendState new_power_state) {
  LOG(INFO) << "Power state changed: "
            << power_state_ << "->" << new_power_state;
  power_state_ = new_power_state;
  OnEvent(new_power_state, &state_change_callbacks_);
}

void PowerManager::RegisterSuspendDelay(const uint32_t &delay_ms) {
  power_manager_proxy_->RegisterSuspendDelay(delay_ms);
}

template<class Callback>
void PowerManager::AddCallback(const string &key, const Callback &callback,
                               map<const string, Callback> *callback_map) {
  CHECK(callback_map != NULL);
  CHECK(!callback.is_null());
  if (ContainsKey(*callback_map, key)) {
    LOG(DFATAL) << "Inserting duplicate key " << key;
    LOG(INFO) << "Removing previous callback for key " << key;
    RemoveCallback(key, callback_map);
  }
  (*callback_map)[key] = callback;
}

template<class Callback>
void PowerManager::RemoveCallback(const string &key,
                                  map<const string, Callback> *callback_map) {
  CHECK(callback_map != NULL);
  DCHECK(ContainsKey(*callback_map, key)) << "Removing unknown key " << key;
  typename map<const std::string, Callback>::iterator it =
      callback_map->find(key);
  if (it != callback_map->end()) {
    callback_map->erase(it);
  }
}

template<class Param, class Callback>
void PowerManager::OnEvent(const Param &param,
                           map<const string, Callback> *callback_map) const {
  CHECK(callback_map != NULL);
  for (typename map<const string, Callback>::const_iterator it =
           callback_map->begin();
       it != callback_map->end(); ++it) {
    CHECK(!it->second.is_null());
    it->second.Run(param);
  }
}



}  // namespace shill
