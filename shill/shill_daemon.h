// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DAEMON_H_
#define SHILL_DAEMON_H_

#include <string>

#include <base/memory/scoped_ptr.h>

#include "shill/config80211.h"
#include "shill/event_dispatcher.h"
#include "shill/glib.h"
#include "shill/manager.h"
#include "shill/metrics.h"
#include "shill/sockets.h"

namespace shill {

class Config;
class ControlInterface;
class DHCPProvider;
class Error;
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
  void SetStartupPortalList(const std::string &portal_list);
  // Main for connection manager.  Starts main process and holds event loop.
  void Run();

  // Starts the termination actions in the manager.
  void Quit();

 private:
  friend class ShillDaemonTest;

  // Time to wait for termination actions to complete.
  static const int kTerminationActionsTimeout;  // ms

  // Causes the dispatcher message loop to terminate, calls Stop(), and returns
  // to the main function which started the daemon.  Called when the termination
  // actions are completed.
  void TerminationActionsCompleted(const Error &error);
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
  Config80211 *config80211_;
  Callback80211Object *callback80211_;
  scoped_ptr<Manager> manager_;
  EventDispatcher dispatcher_;
  Sockets sockets_;
  GLib glib_;
};

}  // namespace shill

#endif  // SHILL_DAEMON_H_
