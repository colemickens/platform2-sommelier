// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerm/powerman.h"

namespace power_manager {

PowerManDaemon::PowerManDaemon() : loop_(NULL) {}

PowerManDaemon::~PowerManDaemon() {}

void PowerManDaemon::Init() {
  loop_ = g_main_loop_new(NULL, false);
}

void PowerManDaemon::Run() {
  g_main_loop_run(loop_);
}

}  // namespace power_manager
