// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wimax_provider.h"

#include <base/bind.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/dbus_properties_proxy_interface.h"
#include "shill/error.h"
#include "shill/proxy_factory.h"
#include "shill/scope_logger.h"
#include "shill/wimax_manager_proxy_interface.h"

using base::Bind;
using base::Unretained;
using std::string;
using std::vector;

namespace shill {

WiMaxProvider::WiMaxProvider(ControlInterface *control,
                             EventDispatcher *dispatcher,
                             Metrics *metrics,
                             Manager *manager)
    : control_(control),
      dispatcher_(dispatcher),
      metrics_(metrics),
      manager_(manager),
      proxy_factory_(ProxyFactory::GetInstance()) {}

WiMaxProvider::~WiMaxProvider() {
  Stop();
}

void WiMaxProvider::Start() {
  SLOG(WiMax, 2) << __func__;
  if (manager_proxy_.get()) {
    return;
  }
  properties_proxy_.reset(
      proxy_factory_->CreateDBusPropertiesProxy(
          wimax_manager::kWiMaxManagerServicePath,
          wimax_manager::kWiMaxManagerServiceName));
  properties_proxy_->set_properties_changed_callback(
      Bind(&WiMaxProvider::OnPropertiesChanged, Unretained(this)));
  manager_proxy_.reset(proxy_factory_->CreateWiMaxManagerProxy());
  Error error;
  UpdateDevices(manager_proxy_->Devices(&error));
}

void WiMaxProvider::Stop() {
  SLOG(WiMax, 2) << __func__;
  properties_proxy_.reset();
  manager_proxy_.reset();
}

void WiMaxProvider::UpdateDevices(const vector<RpcIdentifier> &/*devices*/) {
  SLOG(WiMax, 2) << __func__;
  // TODO(petkov): Create/register and destroy/unregister WiMax devices as
  // necessary.
  NOTIMPLEMENTED();
}

void WiMaxProvider::OnPropertiesChanged(
    const string &/*interface*/,
    const DBusPropertiesMap &/*changed_properties*/,
    const vector<string> &/*invalidated_properties*/) {
  SLOG(WiMax, 2) << __func__;
  // TODO(petkov): Parse the "Devices" property and invoke UpdateDevices.
  NOTIMPLEMENTED();
}

}  // namespace shill
