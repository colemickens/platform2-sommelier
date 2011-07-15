// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DAEMON_H_
#define SHILL_DAEMON_H_

#include <string>

#include "shill/manager.h"
#include "shill/shill_event.h"
#include "shill/sockets.h"

namespace shill {

class Config;
class ControlInterface;
class GLib;

class Daemon {
 public:
  Daemon(Config *config, ControlInterface *control, GLib *glib);
  ~Daemon();

  void AddDeviceToBlackList(const std::string &device_name);
  void Start();
  void Run();

 private:
  friend class ShillDaemonTest;

  ControlInterface *CreateControl();

  Config *config_;
  ControlInterface *control_;
  Manager manager_;
  EventDispatcher dispatcher_;
  Sockets sockets_;
};

}  // namespace shill

#endif  // SHILL_DAEMON_H_
