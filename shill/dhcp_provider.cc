// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dhcp_provider.h"

#include <base/logging.h>

#include "shill/dhcp_config.h"
#include "shill/dhcpcd_proxy.h"
#include "shill/proxy_factory.h"

using std::string;

namespace shill {

DHCPProvider::DHCPProvider() : glib_(NULL) {
  VLOG(2) << __func__;
}

DHCPProvider::~DHCPProvider() {
  VLOG(2) << __func__;
}

DHCPProvider* DHCPProvider::GetInstance() {
  return Singleton<DHCPProvider>::get();
}

void DHCPProvider::Init(GLib *glib) {
  VLOG(2) << __func__;
  listener_.reset(
      new DHCPCDListener(ProxyFactory::factory()->connection(), this));
  glib_ = glib;
}

DHCPConfigRefPtr DHCPProvider::CreateConfig(const string &device_name) {
  VLOG(2) << __func__ << " device: " << device_name;
  return new DHCPConfig(this, device_name, glib_);
}

DHCPConfigRefPtr DHCPProvider::GetConfig(int pid) {
  VLOG(2) << __func__ << " pid: " << pid;
  PIDConfigMap::const_iterator it = configs_.find(pid);
  if (it == configs_.end()) {
    return NULL;
  }
  return it->second;
}

void DHCPProvider::BindPID(int pid, const DHCPConfigRefPtr &config) {
  VLOG(2) << __func__ << " pid: " << pid;
  configs_[pid] = config;
}

void DHCPProvider::UnbindPID(int pid) {
  VLOG(2) << __func__ << " pid: " << pid;
  configs_.erase(pid);
}

}  // namespace shill
