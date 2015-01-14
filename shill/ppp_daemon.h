// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_PPP_DAEMON_H_
#define SHILL_PPP_DAEMON_H_

#include <string>

#include <base/callback.h>
#include <base/memory/weak_ptr.h>
#include <gtest/gtest_prod.h>

#include "shill/external_task.h"

namespace shill {

class ControlInterface;
class GLib;
class Error;

// PPPDaemon provides control over the configuration and instantiation of pppd
// processes.  All pppd instances created through PPPDaemon will use shill's
// pppd plugin.
class PPPDaemon {
 public:
  // The type of callback invoked when an ExternalTask wrapping a pppd instance
  // dies.  The first argument is the pid of the process, the second is the exit
  // code.
  typedef base::Callback<void(pid_t, int)> DeathCallback;

  // Provides options used when preparing a pppd task for execution.  These map
  // to pppd command-line options.  Refer to https://ppp.samba.org/pppd.html for
  // more details about the meaning of each.
  struct Options {
    Options()
        : no_detach(false), no_default_route(false), use_peer_dns(false) {}

    bool no_detach;
    bool no_default_route;  // Don't let pppd muck with routing table.
    bool use_peer_dns;      // Request DNS servers.
  };

  // The path to the pppd plugin provided by shill.
  static const char kPluginPath[];

  // Starts a pppd instance.  |options| provides the configuration for the
  // instance to be started, |device| specifies which device the PPP connection
  // is to be established on, |death_callback| will be invoked when the
  // underlying pppd process dies.  |error| is populated if the task cannot be
  // started, and nullptr is returned.
  static std::unique_ptr<ExternalTask> Start(
      ControlInterface *control_interface,
      GLib *glib,
      const base::WeakPtr<RPCTaskDelegate> &task_delegate,
      const Options &options,
      const std::string &device,
      const DeathCallback &death_callback,
      Error *error);

 private:
  FRIEND_TEST(PPPDaemonTest, PluginUsed);

  static const char kDaemonPath[];

  PPPDaemon();
  ~PPPDaemon();

  DISALLOW_COPY_AND_ASSIGN(PPPDaemon);
};

}  // namespace shill

#endif  // SHILL_PPP_DAEMON_H_
