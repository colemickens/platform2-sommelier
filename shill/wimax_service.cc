// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wimax_service.h"

#include <base/string_util.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/technology.h"
#include "shill/wimax.h"

using std::string;

namespace shill {

WiMaxService::WiMaxService(ControlInterface *control,
                           EventDispatcher *dispatcher,
                           Metrics *metrics,
                           Manager *manager,
                           const WiMaxRefPtr &wimax)
    : Service(control, dispatcher, metrics, manager, Technology::kWiMax),
      wimax_(wimax),
      storage_id_(
          StringToLowerASCII(string(flimflam::kTypeWimax) +
                             "_" +
                             wimax_->address())) {
  // TODO(petkov): Figure a friendly service name.
  set_friendly_name(wimax->link_name());
  set_connectable(true);
}

WiMaxService::~WiMaxService() {}

bool WiMaxService::TechnologyIs(const Technology::Identifier type) const {
  return type == Technology::kWiMax;
}

void WiMaxService::Connect(Error *error) {
  Service::Connect(error);
  wimax_->Connect(error);
}

void WiMaxService::Disconnect(Error *error) {
  Service::Disconnect(error);
  wimax_->Disconnect(error);
}

string WiMaxService::GetStorageIdentifier() const {
  return storage_id_;
}

string WiMaxService::GetDeviceRpcId(Error *error) {
  return wimax_->GetRpcIdentifier();
}

}  // namespace shill
