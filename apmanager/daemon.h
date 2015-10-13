// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APMANAGER_DAEMON_H_
#define APMANAGER_DAEMON_H_

#include <base/callback_forward.h>
#include <brillo/daemons/dbus_daemon.h>

#include "apmanager/manager.h"

namespace apmanager {

class Daemon : public brillo::DBusServiceDaemon {
 public:
  // User and group to run the apmanager process.
  static const char kAPManagerGroupName[];
  static const char kAPManagerUserName[];

  explicit Daemon(const base::Closure& startup_callback);
  ~Daemon() = default;

 protected:
  int OnInit() override;
  void OnShutdown(int* return_code) override;
  void RegisterDBusObjectsAsync(
      brillo::dbus_utils::AsyncEventSequencer* sequencer) override;

 private:
  friend class DaemonTest;

  std::unique_ptr<Manager> manager_;
  base::Closure startup_callback_;

  DISALLOW_COPY_AND_ASSIGN(Daemon);
};

}  // namespace apmanager

#endif  // APMANAGER_DAEMON_H_
