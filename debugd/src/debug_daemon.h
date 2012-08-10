// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEBUG_DAEMON_H
#define DEBUG_DAEMON_H

#include <map>
#include <string>

#include <dbus-c++/dbus.h>

#include "adaptors/org.chromium.debugd.h"
#include "debug_logs_tool.h"
#include "debug_mode_tool.h"
#include "example_tool.h"
#include "log_tool.h"
#include "modem_status_tool.h"
#include "netif_tool.h"
#include "network_status_tool.h"
#include "ping_tool.h"
#include "route_tool.h"
#include "systrace_tool.h"
#include "tracepath_tool.h"

namespace debugd {

class ProcessWithId;

class DebugDaemon : public org::chromium::debugd_adaptor,
                    public DBus::ObjectAdaptor,
                    public DBus::IntrospectableAdaptor {
 public:
  DebugDaemon(DBus::Connection* connection, DBus::BusDispatcher* dispatcher);
  virtual ~DebugDaemon();

  virtual bool Init();
  virtual void Run();

  // Setters for tools - used for dependency injection in tests
  void set_debug_logs_tool(DebugLogsTool* tool) {
    debug_logs_tool_ = tool;
  }
  void set_ping_tool(PingTool* tool) {
    ping_tool_ = tool;
  }
  void set_route_tool(RouteTool* tool) {
    route_tool_ = tool;
  }
  void set_tracepath_tool(TracePathTool* tool) {
    tracepath_tool_ = tool;
  }
  void set_systrace_tool(SystraceTool* tool) {
    systrace_tool_ = tool;
  }
  void set_modem_status_tool(ModemStatusTool* tool) {
    modem_status_tool_ = tool;
  }
  void set_network_status_tool(NetworkStatusTool* tool) {
    network_status_tool_ = tool;
  }
  void set_log_tool(LogTool* tool) {
    log_tool_ = tool;
  }

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
  virtual void SystraceStart(const std::string& categories,
                             DBus::Error& error);
  virtual void SystraceStop(const DBus::FileDescriptor& outfd,
                            DBus::Error& error);
  virtual std::string SystraceStatus(DBus::Error& error); // NOLINT dbuscxx
  virtual std::vector<std::string> GetRoutes(const std::map<std::string,
                                                            DBus::Variant>&
                                                 options,
                                             DBus::Error& error);
  virtual std::string GetModemStatus(DBus::Error& error); // NOLINT
  virtual std::string GetNetworkStatus(DBus::Error& error); // NOLINT
  virtual void GetDebugLogs(const DBus::FileDescriptor& fd,
                            DBus::Error& error);
  virtual void SetDebugMode(const std::string& subsystem, DBus::Error& error);
  virtual std::string GetLog(const std::string& name, DBus::Error& error); // NOLINT
  virtual std::map<std::string, std::string> GetAllLogs(DBus::Error& error); // NOLINT
  virtual std::map<std::string, std::string>
      GetFeedbackLogs(DBus::Error& error); // NOLINT
  virtual std::string GetExample(DBus::Error& error); // NOLINT
  virtual std::string GetInterfaces(DBus::Error& error); // NOLINT

 private:
  DBus::Connection* dbus_;
  DBus::BusDispatcher* dispatcher_;

  DebugLogsTool* debug_logs_tool_;
  DebugModeTool* debug_mode_tool_;
  ExampleTool* example_tool_;
  LogTool* log_tool_;
  ModemStatusTool* modem_status_tool_;
  NetifTool* netif_tool_;
  NetworkStatusTool* network_status_tool_;
  PingTool* ping_tool_;
  RouteTool* route_tool_;
  SystraceTool* systrace_tool_;
  TracePathTool* tracepath_tool_;
};

};  // namespace debugd

#endif  // DEBUG_DAEMON_H
