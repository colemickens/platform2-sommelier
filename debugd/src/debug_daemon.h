// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEBUG_DAEMON_H
#define DEBUG_DAEMON_H

#include <map>
#include <string>

#include <dbus-c++/dbus.h>

#include "bindings/org.chromium.debugd.h"
#include "ping_tool.h"
#include "route_tool.h"
#include "tracepath_tool.h"

namespace debugd {

class ProcessWithId;

class DebugDaemon : public org::chromium::debugd_adaptor,
                    public DBus::ObjectAdaptor,
                    public DBus::IntrospectableAdaptor {
 public:
  DebugDaemon(DBus::Connection* connection, DBus::BusDispatcher* dispatcher);
  ~DebugDaemon();

  bool Init();
  void Run();

  // Public methods below this point are part of the DBus interface presented by
  // this object, and are documented in </share/org.chromium.debugd.xml>.
  virtual std::string PingStart(const DBus::FileDescriptor& outfd,
                                const std::string& dest,
                                const std::map<std::string,
                                               DBus::Variant>& options,
                                DBus::Error& error);
  virtual void PingStop(const std::string& handle, DBus::Error& error);
  virtual std::string TracePathStart(const DBus::FileDescriptor& outfd,
                                     const std::string& destination,
                                     const std::map<std::string,
                                                    DBus::Variant>& options,
                                     DBus::Error& error);
  virtual void TracePathStop(const std::string& handle, DBus::Error& error);
  virtual std::vector<std::string> GetRoutes(const std::map<std::string,
                                                            DBus::Variant>&
                                                 options,
                                             DBus::Error& error);

 private:
  DBus::Connection* dbus_;
  DBus::BusDispatcher* dispatcher_;
  PingTool* ping_tool_;
  RouteTool* route_tool_;
  TracePathTool *tracepath_tool_;
};

};  // namespace debugd

#endif  // DEBUG_DAEMON_H
