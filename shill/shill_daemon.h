// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DAEMON_H_
#define SHILL_DAEMON_H_

#include <string>

#include <base/memory/scoped_ptr.h>

#include "shill/callback80211_metrics.h"
#include "shill/event_dispatcher.h"
#include "shill/glib.h"
#include "shill/manager.h"
#include "shill/sockets.h"

namespace shill {

class Config;
class ControlInterface;
class DHCPProvider;
class Error;
class GLib;
class Metrics;
class NetlinkManager;
class NSS;
class ProxyFactory;
class RoutingTable;
class RTNLHandler;

class Daemon {
 public:
  Daemon(Config *config, ControlInterface *control);
  ~Daemon();

  void AddDeviceToBlackList(const std::string &device_name);
  void SetStartupPortalList(const std::string &portal_list);
  // Main for connection manager.  Starts main process and holds event loop.
  void Run();

  // Starts the termination actions in the manager.
  void Quit();

 private:
  friend class ShillDaemonTest;

  // Called when the termination actions are completed.
  void TerminationActionsCompleted(const Error &error);

  // Calls Stop() and then causes the dispatcher message loop to terminate and
  // return to the main function which started the daemon.
  void StopAndReturnToMain();

  void Start();
  void Stop();

  Config *config_;
  ControlInterface *control_;
  EventDispatcher dispatcher_;
  GLib glib_;
  Sockets sockets_;
  scoped_ptr<Metrics> metrics_;
  NSS *nss_;
  ProxyFactory *proxy_factory_;
  RTNLHandler *rtnl_handler_;
  RoutingTable *routing_table_;
  DHCPProvider *dhcp_provider_;
  NetlinkManager *netlink_manager_;
  scoped_ptr<Manager> manager_;
  Callback80211Metrics callback80211_metrics_;
};

}  // namespace shill

#endif  // SHILL_DAEMON_H_
