// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wimax.h"

#include <base/bind.h>

#include "shill/manager.h"
#include "shill/proxy_factory.h"
#include "shill/scope_logger.h"
#include "shill/wimax_device_proxy_interface.h"
#include "shill/wimax_service.h"

using base::Bind;
using std::string;

namespace shill {

const int WiMax::kTimeoutDefault = 30000;

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
  proxy_->Enable(
      error, Bind(&WiMax::OnEnableComplete, this, callback), kTimeoutDefault);
}

void WiMax::Stop(Error *error, const EnabledStateChangedCallback &callback) {
  SLOG(WiMax, 2) << __func__;
  DestroyIPConfig();
  SelectService(NULL);
  if (service_) {
    manager()->DeregisterService(service_);
    service_ = NULL;
  }
  proxy_->Disable(
      error, Bind(&WiMax::OnDisableComplete, this, callback), kTimeoutDefault);
}

bool WiMax::TechnologyIs(const Technology::Identifier type) const {
  return type == Technology::kWiMax;
}

void WiMax::Connect(Error *error) {
  SLOG(WiMax, 2) << __func__;
  service_->SetState(Service::kStateAssociating);
  proxy_->Connect(
      error, Bind(&WiMax::OnConnectComplete, this), kTimeoutDefault);
}

void WiMax::Disconnect(Error *error) {
  SLOG(WiMax, 2) << __func__;
  proxy_->Disconnect(
      error, Bind(&WiMax::OnDisconnectComplete, this), kTimeoutDefault);
}

void WiMax::OnConnectComplete(const Error &error) {
  SLOG(WiMax, 2) << __func__;
  if (error.IsSuccess() && AcquireIPConfig()) {
    SelectService(service_);
    SetServiceState(Service::kStateConfiguring);
  } else {
    LOG(ERROR) << "Unable to connect.";
    if (service_) {
      service_->SetState(Service::kStateFailure);
    }
  }
}

void WiMax::OnDisconnectComplete(const Error &error) {
  SLOG(WiMax, 2) << __func__;
  DestroyIPConfig();
  SelectService(NULL);
}

void WiMax::OnEnableComplete(const EnabledStateChangedCallback &callback,
                             const Error &error) {
  SLOG(WiMax, 2) << __func__;
  if (error.IsFailure()) {
    proxy_.reset();
  } else {
    service_ = new WiMaxService(control_interface(),
                                dispatcher(),
                                metrics(),
                                manager(),
                                this);
    manager()->RegisterService(service_);
  }
  callback.Run(error);
}

void WiMax::OnDisableComplete(const EnabledStateChangedCallback &callback,
                              const Error &error) {
  SLOG(WiMax, 2) << __func__;
  callback.Run(error);
}

}  // namespace shill
