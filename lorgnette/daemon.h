// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LORGNETTE_DAEMON_H_
#define LORGNETTE_DAEMON_H_

#include <string>

#include <base/cancelable_callback.h>
#include <base/memory/scoped_ptr.h>
#include <base/message_loop/message_loop.h>
#include <dbus-c++/glib-integration.h>
#include <dbus-c++/util.h>

namespace base {
class MessageLoopProxy;
}  // namespace base

namespace lorgnette {

class Manager;

class Daemon {
 public:
  // User and group to run the lorgnette process.
  static const char kScanGroupName[];
  static const char kScanUserName[];

  Daemon();
  ~Daemon();

  // Setup DBus resources for main loop.
  void Start();

  // Run main loop for scan manager.
  void Run();

  // Starts the termination actions in the manager.
  void Quit();

 private:
  friend class DaemonTest;

  // Daemon will automatically shutdown after this length of idle time.
  static const int kShutdownTimeoutMilliseconds;

  // Restarts a timer for the termination of the daemon process.
  void PostponeShutdown();

  scoped_ptr<base::MessageLoop> dont_use_directly_;
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;
  scoped_ptr<DBus::Glib::BusDispatcher> dispatcher_;
  scoped_ptr<DBus::Connection> connection_;
  scoped_ptr<Manager> manager_;
  base::CancelableClosure shutdown_callback_;

  DISALLOW_COPY_AND_ASSIGN(Daemon);
};

}  // namespace lorgnette

#endif  // LORGNETTE_DAEMON_H_
