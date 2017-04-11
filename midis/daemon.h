// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MIDIS_DAEMON_H_
#define MIDIS_DAEMON_H_

#include <memory>

#include <brillo/daemons/daemon.h>

#include "midis/client_tracker.h"
#include "midis/device_tracker.h"

namespace midis {

class Daemon : public brillo::Daemon {
 public:
  Daemon();
  ~Daemon() override;

 protected:
  int OnInit() override;

 private:
  std::unique_ptr<DeviceTracker> device_tracker_;
  std::unique_ptr<ClientTracker> client_tracker_;
  DISALLOW_COPY_AND_ASSIGN(Daemon);
};
}  // namespace midis
#endif  // MIDIS_DAEMON_H_
