// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DAEMON_H_
#define SHILL_DAEMON_H_

#include <string>

#include <base/memory/scoped_ptr.h>

#include "shill/event_dispatcher.h"
#include "shill/glib.h"
#include "shill/manager.h"
#include "shill/metrics.h"
#include "shill/sockets.h"

namespace shill {

class Config;
class ControlInterface;
class DHCPProvider;
class GLib;
class NSS;
class ProxyFactory;
class RoutingTable;
class RTNLHandler;

class Daemon {
 public:
  Daemon(Config *config, ControlInterface *control);
  ~Daemon();

  void AddDeviceToBlackList(const std::string &device_name);
  void SetStartupProfiles(const std::vector<std::string> &profile_path);
  // Main for connection manager.  Starts main process and holds event loop.
  void Run();
  void Quit();

 private:
  friend class ShillDaemonTest;

  void Start();
  void Stop();

  Config *config_;
  ControlInterface *control_;
  Metrics metrics_;
  NSS *nss_;
  ProxyFactory *proxy_factory_;
  RTNLHandler *rtnl_handler_;
  RoutingTable *routing_table_;
  DHCPProvider *dhcp_provider_;
  scoped_ptr<Manager> manager_;
  EventDispatcher dispatcher_;
  Sockets sockets_;
  GLib glib_;
};

}  // namespace shill

#endif  // SHILL_DAEMON_H_
