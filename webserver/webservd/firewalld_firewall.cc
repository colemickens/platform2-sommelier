// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webserver/webservd/firewalld_firewall.h"

#include <string>

#include <base/bind.h>

namespace webservd {

void FirewalldFirewall::WaitForServiceAsync(dbus::Bus* bus,
                                            const base::Closure& callback) {
  service_online_cb_ = callback;
  object_manager_.reset(new org::chromium::Firewalld::ObjectManagerProxy{bus});
  object_manager_->SetFirewalldAddedCallback(
      base::Bind(&FirewalldFirewall::OnFirewalldOnline,
                 weak_ptr_factory_.GetWeakPtr()));
}

void FirewalldFirewall::PunchTcpHoleAsync(
    uint16_t port,
    const std::string& interface_name,
    const base::Callback<void(bool)>& success_cb,
    const base::Callback<void(chromeos::Error*)>& failure_cb) {
  proxy_->PunchTcpHoleAsync(port, interface_name, success_cb, failure_cb);
}

void FirewalldFirewall::OnFirewalldOnline(
    org::chromium::FirewalldProxy* proxy) {
  proxy_ = proxy;
  service_online_cb_.Run();
}

}  // namespace webservd
