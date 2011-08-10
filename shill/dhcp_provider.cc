// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dhcp_provider.h"

#include <base/logging.h>

#include "shill/control_interface.h"
#include "shill/dhcp_config.h"
#include "shill/dhcpcd_proxy.h"
#include "shill/proxy_factory.h"

using std::string;

namespace shill {

static base::LazyInstance<DHCPProvider> g_dhcp_provider(
    base::LINKER_INITIALIZED);

DHCPProvider::DHCPProvider()
    : control_interface_(NULL),
      dispatcher_(NULL),
      glib_(NULL) {
  VLOG(2) << __func__;
}

DHCPProvider::~DHCPProvider() {
  VLOG(2) << __func__;
}

DHCPProvider* DHCPProvider::GetInstance() {
  return g_dhcp_provider.Pointer();
}

void DHCPProvider::Init(ControlInterface *control_interface,
                        EventDispatcher *dispatcher,
                        GLib *glib) {
  VLOG(2) << __func__;
  listener_.reset(
      new DHCPCDListener(ProxyFactory::factory()->connection(), this));
  glib_ = glib;
  control_interface_ = control_interface;
  dispatcher_ = dispatcher;
}

DHCPConfigRefPtr DHCPProvider::CreateConfig(const string &device_name) {
  VLOG(2) << __func__ << " device: " << device_name;
  return new DHCPConfig(
      control_interface_, dispatcher_, this, device_name, glib_);
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
