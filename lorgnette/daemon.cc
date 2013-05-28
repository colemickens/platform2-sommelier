// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "lorgnette/daemon.h"

#include <string>
#include <vector>

#include <base/bind.h>
#include <base/logging.h>
#include <base/message_loop_proxy.h>
#include <base/run_loop.h>

#include "lorgnette/manager.cc"

using std::string;
using std::vector;

namespace lorgnette {

// static
const char Daemon::kInterfaceName[] = "org.chromium.lorgnette";
const char Daemon::kScanGroupName[] = "scanner";
const char Daemon::kScanUserName[] = "saned";
const int Daemon::kShutdownTimeoutMilliseconds = 20000;

Daemon::Daemon()
    : dont_use_directly_(new MessageLoopForUI),
      message_loop_proxy_(base::MessageLoopProxy::current()) {}

Daemon::~Daemon() {}

void Daemon::Run() {
  LOG(INFO) << "Running main loop.";
  MessageLoop::current()->Run();
  LOG(INFO) << "Exited main loop.";
}

void Daemon::Quit() {
  message_loop_proxy_->PostTask(FROM_HERE, MessageLoop::QuitClosure());
}

void Daemon::Start() {
  CHECK(!connection_.get());
  dispatcher_.reset(new DBus::Glib::BusDispatcher());
  CHECK(dispatcher_.get()) << "Failed to create a dbus-dispatcher";
  DBus::default_dispatcher = dispatcher_.get();
  dispatcher_->attach(NULL);
  connection_.reset(new DBus::Connection(DBus::Connection::SystemBus()));
  connection_->request_name(kInterfaceName);
  manager_.reset(new Manager(
      base::Bind(&Daemon::PostponeShutdown, base::Unretained(this))));
  manager_->InitDBus(connection_.get());
  PostponeShutdown();
}

void Daemon::PostponeShutdown() {
  shutdown_callback_.Reset(base::Bind(&Daemon::Quit, base::Unretained(this)));
  message_loop_proxy_->PostDelayedTask(FROM_HERE,
                                       shutdown_callback_.callback(),
                                       base::TimeDelta::FromMilliseconds(
                                           kShutdownTimeoutMilliseconds));
}

}  // namespace lorgnette
