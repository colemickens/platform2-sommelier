// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERM_POWERMAN_H_
#define POWER_MANAGER_POWERM_POWERMAN_H_

#include <glib.h>

namespace power_manager {

class PowerManDaemon {
 public:
  PowerManDaemon();
  virtual ~PowerManDaemon();

  void Init();
  void Run();

 private:
  GMainLoop* loop_;
};

}  // namespace power_manager

#endif  // POWER_MANAGER_POWERM_POWERMAN_H_
