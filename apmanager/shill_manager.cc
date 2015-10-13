// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apmanager/shill_manager.h"

#include <base/bind.h>
#include <brillo/errors/error.h>

#include "apmanager/control_interface.h"

using std::string;

namespace apmanager {

ShillManager::ShillManager() {}

ShillManager::~ShillManager() {}

void ShillManager::Init(ControlInterface* control_interface) {
  CHECK(!shill_proxy_) << "Already init";
  shill_proxy_ =
      control_interface->CreateShillProxy(
          base::Bind(&ShillManager::OnShillServiceAppeared,
                     weak_factory_.GetWeakPtr()),
          base::Bind(&ShillManager::OnShillServiceVanished,
                     weak_factory_.GetWeakPtr()));
}

void ShillManager::ClaimInterface(const string& interface_name) {
  CHECK(shill_proxy_) << "Proxy not initialize yet";
  shill_proxy_->ClaimInterface(interface_name);
  claimed_interfaces_.insert(interface_name);
}

void ShillManager::ReleaseInterface(const string& interface_name) {
  CHECK(shill_proxy_) << "Proxy not initialize yet";
  shill_proxy_->ReleaseInterface(interface_name);
  claimed_interfaces_.erase(interface_name);
}

void ShillManager::OnShillServiceAppeared() {
  LOG(INFO) << __func__;
  // Claim all interfaces from shill service in case this is a new instance.
  for (const auto& interface : claimed_interfaces_) {
    shill_proxy_->ClaimInterface(interface);
  }
}

void ShillManager::OnShillServiceVanished() {
  LOG(INFO) << __func__;
}

}  // namespace apmanager
