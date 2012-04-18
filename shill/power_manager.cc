// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/power_manager.h"

#include <map>
#include <string>

#include <base/logging.h>
#include <base/stl_util.h>

#include "shill/power_manager_proxy_interface.h"
#include "shill/proxy_factory.h"
#include "shill/scope_logger.h"

using std::string;

namespace shill {

PowerManager::PowerManager(ProxyFactory *proxy_factory)
    : power_manager_proxy_(proxy_factory->CreatePowerManagerProxy(this)) {
}

PowerManager::~PowerManager() {
}

void PowerManager::AddStateChangeCallback(const string &key,
                                          const PowerStateCallback &callback) {
  SLOG(Power, 2) << __func__ << " key " << key;
  if (ContainsKey(state_change_callbacks_, key)) {
    LOG(DFATAL) << "Inserting duplicate key " << key;
    LOG(INFO) << "Removing previous callback for key " << key;
    RemoveStateChangeCallback(key);
  }
  state_change_callbacks_[key] = callback;
}

void PowerManager::RemoveStateChangeCallback(const string &key) {
  SLOG(Power, 2) << __func__ << " key " << key;
  DCHECK(ContainsKey(state_change_callbacks_, key)) << "Removing unknown key "
                                                    << key;
  StateChangeCallbackMap::iterator it = state_change_callbacks_.find(key);
  if (it != state_change_callbacks_.end()) {
    state_change_callbacks_.erase(it);
  }
}

void PowerManager::OnSuspendDelay(uint32 /* sequence_number */) {
  // TODO(gmorain): Do something.
}

void PowerManager::OnPowerStateChanged(SuspendState new_power_state) {
  SLOG(Power, 2) << __func__ << " new_power_state " << new_power_state;
  for (StateChangeCallbackMap::const_iterator it =
           state_change_callbacks_.begin();
       it != state_change_callbacks_.end(); ++it) {
    it->second.Run(new_power_state);
  }
}

void PowerManager::RegisterSuspendDelay(const uint32_t & /* delay_ms */) {
  // TODO(gmorain): Dispatch this to the proxy.
}

}  // namespace shill
