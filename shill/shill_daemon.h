// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_SHILL_DAEMON_H_
#define SHILL_SHILL_DAEMON_H_

#include <memory>
#include <string>
#include <vector>

#include "shill/control_interface.h"
#include "shill/event_dispatcher.h"
#include "shill/glib.h"
#include "shill/manager.h"

#if !defined(DISABLE_WIFI)
#include "shill/wifi/callback80211_metrics.h"
#endif  // DISABLE_WIFI

namespace shill {

class Config;
class DHCPProvider;
class Error;
class GLib;
class Metrics;
class RoutingTable;
class RTNLHandler;

#if !defined(DISABLE_WIFI)
class NetlinkManager;
#endif  // DISABLE_WIFI

class Daemon {
 public:
  // Run-time settings retrieved from command line.
  struct Settings {
    Settings()
        : ignore_unknown_ethernet(false),
          minimum_mtu(0),
          passive_mode(false),
          use_portal_list(false) {}
    std::string accept_hostname_from;
    std::string default_technology_order;
    std::vector<std::string> device_blacklist;
    std::vector<std::string> dhcpv6_enabled_devices;
    bool ignore_unknown_ethernet;
    int minimum_mtu;
    bool passive_mode;
    std::string portal_list;
    std::string prepend_dns_servers;
    bool use_portal_list;
  };

  Daemon(Config* config, ControlInterface* control);
  ~Daemon();

  // Apply run-time settings to the manager.
  void ApplySettings(const Settings& settings);

  // Main for connection manager.  Starts main process and holds event loop.
  void Run();

  // Starts the termination actions in the manager.
  void Quit();

 private:
  friend class ShillDaemonTest;

  // Called when the termination actions are completed.
  void TerminationActionsCompleted(const Error& error);

  // Calls Stop() and then causes the dispatcher message loop to terminate and
  // return to the main function which started the daemon.
  void StopAndReturnToMain();

  void Start();
  void Stop();

  Config* config_;
  std::unique_ptr<ControlInterface> control_;
  EventDispatcher dispatcher_;
  GLib glib_;
  std::unique_ptr<Metrics> metrics_;
  RTNLHandler* rtnl_handler_;
  RoutingTable* routing_table_;
  DHCPProvider* dhcp_provider_;
#if !defined(DISABLE_WIFI)
  NetlinkManager* netlink_manager_;
  Callback80211Metrics callback80211_metrics_;
#endif  // DISABLE_WIFI
  std::unique_ptr<Manager> manager_;
};

}  // namespace shill

#endif  // SHILL_SHILL_DAEMON_H_
