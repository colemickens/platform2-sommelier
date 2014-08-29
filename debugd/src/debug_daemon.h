// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEBUGD_SRC_DEBUG_DAEMON_H_
#define DEBUGD_SRC_DEBUG_DAEMON_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include <dbus-c++/dbus.h>

#include "debugd/dbus_adaptors/org.chromium.debugd.h"
#include "debugd/src/crash_sender_tool.h"
#include "debugd/src/debug_logs_tool.h"
#include "debugd/src/debug_mode_tool.h"
#include "debugd/src/example_tool.h"
#include "debugd/src/icmp_tool.h"
#include "debugd/src/log_tool.h"
#include "debugd/src/memory_tool.h"
#include "debugd/src/modem_status_tool.h"
#include "debugd/src/netif_tool.h"
#include "debugd/src/network_status_tool.h"
#include "debugd/src/packet_capture_tool.h"
#include "debugd/src/perf_tool.h"
#include "debugd/src/ping_tool.h"
#include "debugd/src/route_tool.h"
#include "debugd/src/storage_tool.h"
#include "debugd/src/sysrq_tool.h"
#include "debugd/src/systrace_tool.h"
#include "debugd/src/tracepath_tool.h"
#include "debugd/src/wimax_status_tool.h"

namespace debugd {

class ProcessWithId;

class DebugDaemon : public org::chromium::debugd_adaptor,
                    public DBus::ObjectAdaptor,
                    public DBus::IntrospectableAdaptor {
 public:
  DebugDaemon(DBus::Connection* connection, DBus::BusDispatcher* dispatcher);
  ~DebugDaemon() override = default;

  bool Init();
  void Run();

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
  void set_wimax_status_tool(WiMaxStatusTool* tool) {
    wimax_status_tool_ = tool;
  }
  void set_log_tool(LogTool* tool) {
    log_tool_ = tool;
  }

  // Public methods below this point are part of the DBus interface presented by
  // this object, and are documented in </share/org.chromium.debugd.xml>.
  std::string PingStart(const DBus::FileDescriptor& outfd,
                        const std::string& dest,
                        const std::map<std::string, DBus::Variant>& options,
                        DBus::Error& error) override;  // NOLINT
  void PingStop(const std::string& handle,
                DBus::Error& error) override;  // NOLINT
  std::string TracePathStart(
      const DBus::FileDescriptor& outfd,
      const std::string& destination,
      const std::map<std::string, DBus::Variant>& options,
      DBus::Error& error) override;  // NOLINT
  void TracePathStop(const std::string& handle,
                     DBus::Error& error) override;  // NOLINT
  void SystraceStart(const std::string& categories,
                     DBus::Error& error) override;  // NOLINT
  void SystraceStop(const DBus::FileDescriptor& outfd,
                    DBus::Error& error) override;  // NOLINT
  std::string SystraceStatus(DBus::Error& error) override;  // NOLINT
  std::vector<std::string> GetRoutes(
      const std::map<std::string, DBus::Variant>& options,
      DBus::Error& error) override;  // NOLINT
  std::string GetModemStatus(DBus::Error& error) override;  // NOLINT
  std::string RunModemCommand(const std::string& command,
                              DBus::Error& error) override;  // NOLINT
  std::string GetNetworkStatus(DBus::Error& error) override;  // NOLINT
  std::string GetWiMaxStatus(DBus::Error& error) override;  // NOLINT
  std::vector<uint8_t> GetRichPerfData(const uint32_t& duration,
                                       DBus::Error& error) override;  // NOLINT
  void GetDebugLogs(const DBus::FileDescriptor& fd,
                    DBus::Error& error) override;  // NOLINT
  void DumpDebugLogs(const bool& is_compressed,
                     const DBus::FileDescriptor& fd,
                     DBus::Error& error) override;  // NOLINT
  void SetDebugMode(const std::string& subsystem,
                    DBus::Error& error) override;  // NOLINT
  std::string GetLog(const std::string& name,
                     DBus::Error& error) override;  // NOLINT
  std::map<std::string, std::string> GetAllLogs(
      DBus::Error& error) override;  // NOLINT
  std::map<std::string, std::string> GetFeedbackLogs(
      DBus::Error& error) override;  // NOLINT
  std::map<std::string, std::string> GetUserLogFiles(
      DBus::Error& error) override;  // NOLINT
  std::string GetExample(DBus::Error& error) override;  // NOLINT
  std::string GetInterfaces(DBus::Error& error) override;  // NOLINT
  std::string TestICMP(const std::string& host,
                       DBus::Error& error) override;  // NOLINT
  std::string TestICMPWithOptions(
      const std::string& host,
      const std::map<std::string, std::string>& options,
      DBus::Error& error) override;  // NOLINT
  std::string Smartctl(const std::string& option,
                       DBus::Error& error) override;  // NOLINT
  std::string MemtesterStart(const DBus::FileDescriptor& outfd,
                             const uint32_t& memory,
                             DBus::Error& error) override;  // NOLINT
  void MemtesterStop(const std::string& handle,
                     DBus::Error& error) override;  // NOLINT
  std::string BadblocksStart(const DBus::FileDescriptor& outfd,
                             DBus::Error& error) override;  // NOLINT
  void BadblocksStop(const std::string& handle,
                     DBus::Error& error) override;  // NOLINT
  std::string PacketCaptureStart(
      const DBus::FileDescriptor& statfd,
      const DBus::FileDescriptor& outfd,
      const std::map<std::string, DBus::Variant>& options,
      DBus::Error& error) override;  // NOLINT
  void PacketCaptureStop(const std::string& handle,
                         DBus::Error& error) override;  // NOLINT
  void LogKernelTaskStates(DBus::Error& error) override;  // NOLINT
  void UploadCrashes(DBus::Error& error) override;  // NOLINT

 private:
  DBus::Connection* dbus_;
  DBus::BusDispatcher* dispatcher_;

  CrashSenderTool* crash_sender_tool_;
  DebugLogsTool* debug_logs_tool_;
  DebugModeTool* debug_mode_tool_;
  ExampleTool* example_tool_;
  ICMPTool* icmp_tool_;
  LogTool* log_tool_;
  MemtesterTool* memory_tool_;
  ModemStatusTool* modem_status_tool_;
  NetifTool* netif_tool_;
  NetworkStatusTool* network_status_tool_;
  PacketCaptureTool* packet_capture_tool_;
  PerfTool* perf_tool_;
  PingTool* ping_tool_;
  RouteTool* route_tool_;
  StorageTool* storage_tool_;
  SysrqTool* sysrq_tool_;
  SystraceTool* systrace_tool_;
  TracePathTool* tracepath_tool_;
  WiMaxStatusTool* wimax_status_tool_;
};

}  // namespace debugd

#endif  // DEBUGD_SRC_DEBUG_DAEMON_H_
