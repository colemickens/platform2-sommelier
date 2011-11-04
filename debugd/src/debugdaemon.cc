// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugdaemon.h"

static const char *kDebugDaemonPath = "/org/chromium/debugd";

DebugDaemon::DebugDaemon(DBus::Connection* connection)
  : DBus::ObjectAdaptor(*connection, kDebugDaemonPath),
    dbus_(connection) { }

DebugDaemon::~DebugDaemon() { }

void DebugDaemon::Run() {
  dispatcher_.enter();
  while (1) {
    dispatcher_.do_iteration();
  }
  // Unreachable.
  dispatcher_.leave();
}

// Methods below this point are interface methods of the DBus interface we
// present, and are inherited from org::chromium::debugd_adaptor. They are
// documented in </share/org.chromium.debugd.xml>, and the generated code is in
// </src/bindings/org.chromium.debugd.h>.
std::string DebugDaemon::PingStart(const int& outfd,
                                   const std::string& destination,
                                   const std::map<std::string, DBus::Variant>&
                                       options,
                                   DBus::Error& error) {
  // TODO(ellyjones): implement me!
  return "";
}

void DebugDaemon::PingStop(const std::string& handle, DBus::Error& error) {
  // TODO(ellyjones): implement me!
  return;
}
