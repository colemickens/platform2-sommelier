// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dhcp_config.h"

#include <base/logging.h>
#include <glib.h>

#include "shill/dhcpcd_proxy.h"
#include "shill/dhcp_provider.h"

namespace shill {

const char DHCPConfig::kDHCPCDPath[] = "/sbin/dhcpcd";

DHCPConfig::DHCPConfig(DHCPProvider *provider, const Device &device)
    : IPConfig(device),
      provider_(provider),
      pid_(0) {
  VLOG(2) << __func__ << ": " << GetDeviceName();
}

DHCPConfig::~DHCPConfig() {
  VLOG(2) << __func__ << ": " << GetDeviceName();
}

bool DHCPConfig::Request() {
  VLOG(2) << __func__ << ": " << GetDeviceName();
  if (!pid_) {
    return Start();
  }
  if (!proxy_.get()) {
    LOG(ERROR)
        << "Unable to acquire destination address before receiving request.";
    return false;
  }
  return Renew();
}

bool DHCPConfig::Renew() {
  VLOG(2) << __func__ << ": " << GetDeviceName();
  if (!pid_ || !proxy_.get()) {
    return false;
  }
  proxy_->DoRebind(GetDeviceName());
  return true;
}

void DHCPConfig::InitProxy(DBus::Connection *connection, const char *service) {
  if (!proxy_.get()) {
    proxy_.reset(new DHCPCDProxy(connection, service));
  }
}

bool DHCPConfig::Start() {
  VLOG(2) << __func__ << ": " << GetDeviceName();

  char *argv[4], *envp[1];
  argv[0] = const_cast<char *>(kDHCPCDPath);
  argv[1] = const_cast<char *>("-B");  // foreground
  argv[2] = const_cast<char *>(GetDeviceName().c_str());
  argv[3] = NULL;

  envp[0] = NULL;

  GPid pid = 0;
  if (!g_spawn_async(NULL,
                     argv,
                     envp,
                     G_SPAWN_DO_NOT_REAP_CHILD,
                     NULL,
                     NULL,
                     &pid,
                     NULL)) {
    LOG(ERROR) << "Unable to spawn " << kDHCPCDPath;
    return false;
  }
  pid_ = pid;
  LOG(INFO) << "Spawned " << kDHCPCDPath << " with pid: " << pid_;
  provider_->BindPID(pid_, DHCPConfigRefPtr(this));
  // TODO(petkov): Add an exit watch to cleanup w/ g_spawn_close_pid.
  return true;
}

}  // namespace shill
