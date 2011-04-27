// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_H_
#define SHILL_H_

#include <string>

#include <ctime>

#include "shill/shill_config.h"
#include "shill/control_interface.h"
#include "shill/shill_event.h"
#include "shill/manager.h"

namespace shill {

class Daemon {
 public:
  explicit Daemon(Config *config, ControlInterface *control);
  ~Daemon();
  void Run();

 private:
  ControlInterface *CreateControl();
  Config *config_;
  ControlInterface *control_;
  Manager manager_;
  EventDispatcher dispatcher_;
  friend class ShillDaemonTest;
};

}  // namespace shill

#endif  // SHILL_H_
