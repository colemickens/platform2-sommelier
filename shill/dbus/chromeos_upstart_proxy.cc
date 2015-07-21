// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dbus/chromeos_upstart_proxy.h"

#include <base/bind.h>

#include "shill/logging.h"

using std::string;
using std::vector;

namespace shill {

// static.
const char ChromeosUpstartProxy::kServiceName[] = "com.ubuntu.Upstart";

ChromeosUpstartProxy::ChromeosUpstartProxy(const scoped_refptr<dbus::Bus>& bus)
    : upstart_proxy_(
        new com::ubuntu::Upstart0_6Proxy(bus, kServiceName)) {}

void ChromeosUpstartProxy::EmitEvent(
    const string& name, const vector<string>& env, bool wait) {
  upstart_proxy_->EmitEventAsync(
      name,
      env,
      wait,
      base::Bind(&ChromeosUpstartProxy::OnEmitEventSuccess,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&ChromeosUpstartProxy::OnEmitEventFailure,
                 weak_factory_.GetWeakPtr()));
}

void ChromeosUpstartProxy::OnEmitEventSuccess() {
  VLOG(2) << "Event emitted successful";
}

void ChromeosUpstartProxy::OnEmitEventFailure(chromeos::Error* error) {
  LOG(ERROR) << "Failed to emit event: " << error->GetCode()
      << " " << error->GetMessage();
}

}  // namespace shill
