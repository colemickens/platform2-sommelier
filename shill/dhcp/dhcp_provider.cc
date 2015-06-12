// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dhcp/dhcp_provider.h"

#include <base/bind.h>
#include <base/files/file_util.h>
#include <base/stl_util.h>
#include <base/strings/stringprintf.h>

#include "shill/control_interface.h"
#include "shill/dhcp/dhcpcd_proxy.h"
#include "shill/dhcp/dhcpv4_config.h"
#ifndef DISABLE_DHCPV6
#include "shill/dhcp/dhcpv6_config.h"
#endif
#include "shill/event_dispatcher.h"
#include "shill/logging.h"
#include "shill/shared_dbus_connection.h"

using base::FilePath;
using std::string;

namespace shill {

namespace Logging {
static auto kModuleLogScope = ScopeLogger::kDHCP;
static string ObjectID(DHCPProvider *d) { return "(dhcp_provider)"; }
}

namespace {
base::LazyInstance<DHCPProvider> g_dhcp_provider = LAZY_INSTANCE_INITIALIZER;
static const int kUnbindDelayMilliseconds = 2000;
}  // namespace

constexpr char DHCPProvider::kDHCPCDPathFormatLease[];
#ifndef DISABLE_DHCPV6
constexpr char DHCPProvider::kDHCPCDPathFormatLease6[];
#endif  // DISABLE_DHCPV6

DHCPProvider::DHCPProvider()
    : shared_dbus_connection_(SharedDBusConnection::GetInstance()),
      root_("/"),
      control_interface_(nullptr),
      dispatcher_(nullptr),
      glib_(nullptr),
      metrics_(nullptr) {
  SLOG(this, 2) << __func__;
}

DHCPProvider::~DHCPProvider() {
  SLOG(this, 2) << __func__;
}

DHCPProvider* DHCPProvider::GetInstance() {
  return g_dhcp_provider.Pointer();
}

void DHCPProvider::Init(ControlInterface *control_interface,
                        EventDispatcher *dispatcher,
                        GLib *glib,
                        Metrics *metrics) {
  SLOG(this, 2) << __func__;
  DBus::Connection *connection = shared_dbus_connection_->GetProxyConnection();
  listener_.reset(new DHCPCDListener(connection, this));
  glib_ = glib;
  control_interface_ = control_interface;
  dispatcher_ = dispatcher;
  metrics_ = metrics;
}

void DHCPProvider::Stop() {
  listener_.reset();
}

DHCPConfigRefPtr DHCPProvider::CreateIPv4Config(
    const string &device_name,
    const string &host_name,
    const string &lease_file_suffix,
    bool arp_gateway) {
  SLOG(this, 2) << __func__ << " device: " << device_name;
  return new DHCPv4Config(control_interface_,
                          dispatcher_,
                          this,
                          device_name,
                          host_name,
                          lease_file_suffix,
                          arp_gateway,
                          glib_,
                          metrics_);
}

#ifndef DISABLE_DHCPV6
DHCPConfigRefPtr DHCPProvider::CreateIPv6Config(
    const string &device_name, const string &lease_file_suffix) {
  SLOG(this, 2) << __func__ << " device: " << device_name;
  return new DHCPv6Config(control_interface_,
                          dispatcher_,
                          this,
                          device_name,
                          lease_file_suffix,
                          glib_);
}
#endif

DHCPConfigRefPtr DHCPProvider::GetConfig(int pid) {
  SLOG(this, 2) << __func__ << " pid: " << pid;
  PIDConfigMap::const_iterator it = configs_.find(pid);
  if (it == configs_.end()) {
    return nullptr;
  }
  return it->second;
}

void DHCPProvider::BindPID(int pid, const DHCPConfigRefPtr &config) {
  SLOG(this, 2) << __func__ << " pid: " << pid;
  configs_[pid] = config;
}

void DHCPProvider::UnbindPID(int pid) {
  SLOG(this, 2) << __func__ << " pid: " << pid;
  configs_.erase(pid);
  recently_unbound_pids_.insert(pid);
  dispatcher_->PostDelayedTask(base::Bind(&DHCPProvider::RetireUnboundPID,
                                          base::Unretained(this),
                                          pid), kUnbindDelayMilliseconds);
}

void DHCPProvider::RetireUnboundPID(int pid) {
  recently_unbound_pids_.erase(pid);
}

bool DHCPProvider::IsRecentlyUnbound(int pid) {
  return ContainsValue(recently_unbound_pids_, pid);
}

void DHCPProvider::DestroyLease(const string &name) {
  SLOG(this, 2) << __func__ << " name: " << name;
  base::DeleteFile(root_.Append(
      base::StringPrintf(kDHCPCDPathFormatLease,
                         name.c_str())), false);
#ifndef DISABLE_DHCPV6
  base::DeleteFile(root_.Append(
      base::StringPrintf(kDHCPCDPathFormatLease6,
                         name.c_str())), false);
#endif  // DISABLE_DHCPV6
}

}  // namespace shill
