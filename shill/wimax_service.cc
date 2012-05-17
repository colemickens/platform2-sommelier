// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wimax_service.h"

#include <algorithm>

#include <base/string_util.h>
#include <base/stringprintf.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/scope_logger.h"
#include "shill/technology.h"
#include "shill/wimax.h"
#include "shill/wimax_network_proxy_interface.h"

using std::replace_if;
using std::string;

namespace shill {

WiMaxService::WiMaxService(ControlInterface *control,
                           EventDispatcher *dispatcher,
                           Metrics *metrics,
                           Manager *manager,
                           const WiMaxRefPtr &wimax)
    : Service(control, dispatcher, metrics, manager, Technology::kWiMax),
      wimax_(wimax),
      network_identifier_(0) {}

WiMaxService::~WiMaxService() {}

bool WiMaxService::Start(WiMaxNetworkProxyInterface *proxy) {
  SLOG(WiMax, 2) << __func__;
  proxy_.reset(proxy);

  Error error;
  network_name_ = proxy_->Name(&error);
  if (!error.IsSuccess()) {
    return false;
  }
  network_identifier_ = proxy_->Identifier(&error);
  if (!error.IsSuccess()) {
    return false;
  }

  set_friendly_name(network_name_);
  storage_id_ =
      StringToLowerASCII(base::StringPrintf("%s_%s_%08x_%s",
                                            flimflam::kTypeWimax,
                                            network_name_.c_str(),
                                            network_identifier_,
                                            wimax_->address().c_str()));
  replace_if(
      storage_id_.begin(), storage_id_.end(), &Service::IllegalChar, '_');
  set_connectable(true);
  return true;
}

bool WiMaxService::TechnologyIs(const Technology::Identifier type) const {
  return type == Technology::kWiMax;
}

void WiMaxService::Connect(Error *error) {
  Service::Connect(error);
  wimax_->ConnectTo(this, error);
}

void WiMaxService::Disconnect(Error *error) {
  Service::Disconnect(error);
  wimax_->DisconnectFrom(this, error);
}

string WiMaxService::GetStorageIdentifier() const {
  return storage_id_;
}

string WiMaxService::GetDeviceRpcId(Error *error) {
  return wimax_->GetRpcIdentifier();
}

}  // namespace shill
