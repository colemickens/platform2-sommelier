// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wimax_service.h"

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
      wimax_(wimax) {
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
  // TODO(benchan,petkov): Generate a proper storage identifier.
  return "";
}

string WiMaxService::GetDeviceRpcId(Error *error) {
  // TODO(benchan,petkov): Is this need?
  return "";
}

}  // namespace shill
