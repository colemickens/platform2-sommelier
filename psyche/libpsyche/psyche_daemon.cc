// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "psyche/libpsyche/psyche_daemon.h"

#include <base/logging.h>
#include <protobinder/iservice_manager.h>

namespace psyche {

PsycheDaemon::PsycheDaemon() : psyche_connection_(new PsycheConnection()) {}

PsycheDaemon::~PsycheDaemon() {}

int PsycheDaemon::OnInit() {
  int return_code = BinderDaemon::OnInit();
  if (return_code != 0) {
    LOG(ERROR) << "Error initializing Daemon.";
    return return_code;
  }
  if (!psyche_connection_->Init()) {
    LOG(ERROR) << "Error connecting to psyche.";
    return -1;
  }
  return 0;
}

}  // namespace psyche
