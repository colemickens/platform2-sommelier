// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debug_daemon.h"

#include <base/logging.h>
#include <dbus-c++/dbus.h>

namespace debugd {

static const char* kDebugDaemonService = "org.chromium.debugd";
static const char* kDebugDaemonPath = "/org/chromium/debugd";

DebugDaemon::DebugDaemon(DBus::Connection* connection,
                         DBus::BusDispatcher* dispatcher)
  : DBus::ObjectAdaptor(*connection, kDebugDaemonPath),
    dbus_(connection), dispatcher_(dispatcher) { }

bool DebugDaemon::Init() {
  ping_tool_ = new PingTool();
  tracepath_tool_ = new TracePathTool();
  route_tool_ = new RouteTool();
  try {
    // TODO(ellyjones): Remove this when crosbug.com/23964 is fixed
    dbus_->request_name(kDebugDaemonService);
  }
  catch (DBus::Error e) { // NOLINT
    return false;
  }
  return true;
}

DebugDaemon::~DebugDaemon() { }

void DebugDaemon::Run() {
  dispatcher_->enter();
  while (1) {
    dispatcher_->do_iteration();
  }
  // Unreachable.
  dispatcher_->leave();
}

std::string DebugDaemon::PingStart(const DBus::FileDescriptor& outfd,
                                   const std::string& destination,
                                   const std::map<std::string, DBus::Variant>&
                                       options,
                                   DBus::Error& error) {
  return ping_tool_->Start(outfd, destination, options, error);
}

void DebugDaemon::PingStop(const std::string& handle, DBus::Error& error) {
  return ping_tool_->Stop(handle, error);
}

std::string DebugDaemon::TracePathStart(const DBus::FileDescriptor& outfd,
                                        const std::string& destination,
                                        const std::map<std::string,
                                                       DBus::Variant>& options,
                                        DBus::Error& error) {
  return tracepath_tool_->Start(outfd, destination, options, error);
}

void DebugDaemon::TracePathStop(const std::string& handle, DBus::Error& error) {
  return tracepath_tool_->Stop(handle, error);
}

std::vector<std::string> DebugDaemon::GetRoutes(const std::map<std::string,
                                                               DBus::Variant>&
                                                    options,
                                                DBus::Error& error) {
  return route_tool_->GetRoutes(options, error);
}

};  // namespace debugd
