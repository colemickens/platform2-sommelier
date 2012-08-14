// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
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
  debug_logs_tool_ = new DebugLogsTool();
  debug_mode_tool_ = new DebugModeTool(dbus_);
  example_tool_ = new ExampleTool();
  icmp_tool_ = new ICMPTool();
  modem_status_tool_ = new ModemStatusTool();
  netif_tool_ = new NetifTool();
  network_status_tool_ = new NetworkStatusTool();
  ping_tool_ = new PingTool();
  route_tool_ = new RouteTool();
  systrace_tool_ = new SystraceTool();
  tracepath_tool_ = new TracePathTool();
  log_tool_ = new LogTool();
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

void DebugDaemon::SystraceStart(const std::string& categories,
                                DBus::Error& error) {
  (void) systrace_tool_->Start(categories, error);
}

void DebugDaemon::SystraceStop(const DBus::FileDescriptor& outfd,
    DBus::Error& error) {
  return systrace_tool_->Stop(outfd, error);
}

std::string DebugDaemon::SystraceStatus(DBus::Error& error) { // NOLINT dbuscxx
  return systrace_tool_->Status(error);
}

std::vector<std::string> DebugDaemon::GetRoutes(const std::map<std::string,
                                                               DBus::Variant>&
                                                    options,
                                                DBus::Error& error) {
  return route_tool_->GetRoutes(options, error);
}

std::string DebugDaemon::GetModemStatus(DBus::Error& error) { // NOLINT dbuscxx
  return modem_status_tool_->GetModemStatus(error);
}

std::string DebugDaemon::GetNetworkStatus(DBus::Error& error) { // NOLINT
  return network_status_tool_->GetNetworkStatus(error);
}

void DebugDaemon::GetDebugLogs(const DBus::FileDescriptor& fd,
                                DBus::Error& error) {
  debug_logs_tool_->GetDebugLogs(fd, error);
}

void DebugDaemon::SetDebugMode(const std::string& subsystem,
                               DBus::Error& error) {
  debug_mode_tool_->SetDebugMode(subsystem, &error);
}

std::string DebugDaemon::GetLog(const std::string& name, DBus::Error& error) {
  return log_tool_->GetLog(name, error);
}

std::map<std::string, std::string> DebugDaemon::GetAllLogs(DBus::Error& error) { // NOLINT dbuscxx
  return log_tool_->GetAllLogs(error);
}

std::map<std::string, std::string> DebugDaemon::GetFeedbackLogs(
    DBus::Error& error) { // NOLINT dbuscxx
  return log_tool_->GetFeedbackLogs(error);
}

std::string DebugDaemon::GetExample(DBus::Error& error) { // NOLINT dbuscxx
  return example_tool_->GetExample(error);
}

std::string DebugDaemon::GetInterfaces(DBus::Error& error) { // NOLINT dbuscxx
  return netif_tool_->GetInterfaces(&error);
}

std::string DebugDaemon::TestICMP(const std::string& host, DBus::Error& error) { // NOLINT dbuscxx
  return icmp_tool_->TestICMP(host, &error);
}

};  // namespace debugd
