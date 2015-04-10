// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "psyche/lib/psyche/psyche_daemon.h"

#include <sysexits.h>

#include <base/logging.h>
#include <protobinder/binder_watcher.h>

#include "psyche/lib/psyche/psyche_connection.h"

namespace psyche {

PsycheDaemon::PsycheDaemon() = default;

PsycheDaemon::~PsycheDaemon() = default;

int PsycheDaemon::OnInit() {
  int return_code = chromeos::Daemon::OnInit();
  if (return_code != 0) {
    LOG(ERROR) << "Error initializing Daemon";
    return return_code;
  }
  binder_watcher_.reset(new protobinder::BinderWatcher);
  psyche_connection_.reset(new PsycheConnection);
  if (!psyche_connection_->Init()) {
    LOG(ERROR) << "Error connecting to psyche";
    return EX_UNAVAILABLE;
  }
  return EX_OK;
}

}  // namespace psyche
