// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wimax.h"

#include "shill/proxy_factory.h"
#include "shill/scope_logger.h"
#include "shill/wimax_device_proxy_interface.h"

using std::string;

namespace shill {

WiMax::WiMax(ControlInterface *control,
             EventDispatcher *dispatcher,
             Metrics *metrics,
             Manager *manager,
             const string &link_name,
             const string &address,
             int interface_index,
             const RpcIdentifier &path)
    : Device(control, dispatcher, metrics, manager, link_name, address,
             interface_index, Technology::kWiMax),
      path_(path),
      proxy_factory_(ProxyFactory::GetInstance()) {
  SLOG(WiMax, 2) << __func__ << "(" << link_name << ", " << path << ")";
}

WiMax::~WiMax() {
  SLOG(WiMax, 2) << __func__ << "(" << link_name() << ", " << path_ << ")";
}

void WiMax::Start(Error *error, const EnabledStateChangedCallback &callback) {
  SLOG(WiMax, 2) << __func__;
  proxy_.reset(proxy_factory_->CreateWiMaxDeviceProxy(path_));
  if (error) {
    error->Reset();
  }
}

void WiMax::Stop(Error *error, const EnabledStateChangedCallback &callback) {
  SLOG(WiMax, 2) << __func__;
  proxy_.reset();
  if (error) {
    error->Reset();
  }
}

bool WiMax::TechnologyIs(const Technology::Identifier type) const {
  return type == Technology::kWiMax;
}

void WiMax::Connect(Error *error) {
}

void WiMax::Disconnect(Error *error) {
}

}  // namespace shill
