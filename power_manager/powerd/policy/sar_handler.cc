// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/policy/sar_handler.h"

#include <memory>
#include <utility>

#include <base/logging.h>

#include "power_manager/powerd/policy/wifi_controller.h"
#include "power_manager/powerd/system/user_proximity_watcher_interface.h"

namespace power_manager {
namespace policy {

SarHandler::SarHandler() = default;

SarHandler::~SarHandler() {
  if (user_proximity_watcher_)
    user_proximity_watcher_->RemoveObserver(this);
}

bool SarHandler::Init(system::UserProximityWatcherInterface* user_prox_watcher,
                      Delegate* wifi_delegate,
                      Delegate* lte_delegate) {
  DCHECK(user_prox_watcher);

  wifi_delegate_ = wifi_delegate;
  lte_delegate_ = lte_delegate;

  user_proximity_watcher_ = user_prox_watcher;
  user_proximity_watcher_->AddObserver(this);

  return true;
}

void SarHandler::OnNewSensor(int id, uint32_t roles) {
  // It is in general not possible to figure out the initial proximity state
  // in all cases (e.g. the sensor may have never fired an interrupt thus far).
  // We take a cautious stance here and decide that - until a FAR value is
  // returned to us via IIO event - the proximity for a new sensor is NEAR.
  constexpr UserProximity default_initial_proximity = UserProximity::NEAR;

  const bool includes_wifi =
      (roles & system::UserProximityObserver::SensorRole::SENSOR_ROLE_WIFI) !=
      0;
  const bool includes_lte =
      (roles & system::UserProximityObserver::SensorRole::SENSOR_ROLE_LTE) != 0;

  LOG(INFO) << "New proximity sensor with id " << id << ": "
            << (includes_wifi ? " wifi" : "") << (includes_lte ? " lte" : "");

  if (includes_wifi && wifi_delegate_) {
    wifi_proximity_voting_.Vote(id, default_initial_proximity);
    wifi_delegate_->ProximitySensorDetected(default_initial_proximity);
  }

  if (includes_lte && lte_delegate_) {
    lte_proximity_voting_.Vote(id, default_initial_proximity);
    lte_delegate_->ProximitySensorDetected(default_initial_proximity);
  }

  if (includes_wifi || includes_lte) {
    sensor_roles_.emplace(id, roles);
  } else {
    LOG(WARNING) << "Detected a sensor that does not act upon any subsystem";
  }
}

void SarHandler::OnProximityEvent(int id, UserProximity value) {
  auto iter = sensor_roles_.find(id);
  if (iter == sensor_roles_.end()) {
    // This sensor is not handling any subsystem of interest. Ignore.
    return;
  }

  const bool includes_wifi =
      (iter->second &
       system::UserProximityObserver::SensorRole::SENSOR_ROLE_WIFI) != 0;
  const bool includes_lte =
      (iter->second &
       system::UserProximityObserver::SensorRole::SENSOR_ROLE_LTE) != 0;

  const bool did_wifi_change =
      includes_wifi ? wifi_proximity_voting_.Vote(id, value) : false;
  const bool did_lte_change =
      includes_lte ? lte_proximity_voting_.Vote(id, value) : false;

  if (did_wifi_change && wifi_delegate_) {
    UserProximity wifi_proximity = wifi_proximity_voting_.GetVote();
    wifi_delegate_->HandleProximityChange(wifi_proximity);
  }
  if (did_lte_change && lte_delegate_) {
    UserProximity lte_proximity = lte_proximity_voting_.GetVote();
    lte_delegate_->HandleProximityChange(lte_proximity);
  }
}

}  // namespace policy
}  // namespace power_manager
