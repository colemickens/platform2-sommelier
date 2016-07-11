// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEBUGD_SRC_DEBUG_DAEMON_H_
#define DEBUGD_SRC_DEBUG_DAEMON_H_

#include <stdint.h>
#include <sys/types.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <dbus-c++/dbus.h>

#include "debugd/dbus_adaptors/org.chromium.debugd.h"
#include "debugd/src/battery_tool.h"
#include "debugd/src/container_tool.h"
#include "debugd/src/crash_sender_tool.h"
#include "debugd/src/cups_tool.h"
#include "debugd/src/debug_logs_tool.h"
#include "debugd/src/debug_mode_tool.h"
#include "debugd/src/dev_features_tool.h"
#include "debugd/src/example_tool.h"
#include "debugd/src/icmp_tool.h"
#include "debugd/src/log_tool.h"
#include "debugd/src/memory_tool.h"
#include "debugd/src/modem_status_tool.h"
#include "debugd/src/netif_tool.h"
#include "debugd/src/network_status_tool.h"
#include "debugd/src/oom_adj_tool.h"
#include "debugd/src/packet_capture_tool.h"
#include "debugd/src/perf_tool.h"
#include "debugd/src/ping_tool.h"
#include "debugd/src/restricted_tool_wrapper.h"
#include "debugd/src/route_tool.h"
#include "debugd/src/session_manager_proxy.h"
#include "debugd/src/storage_tool.h"
#include "debugd/src/swap_tool.h"
#include "debugd/src/sysrq_tool.h"
#include "debugd/src/systrace_tool.h"
#include "debugd/src/tracepath_tool.h"
#include "debugd/src/wifi_debug_tool.h"
#include "debugd/src/wimax_status_tool.h"

namespace debugd {

class DebugDaemon : public org::chromium::debugd_adaptor,
                    public DBus::ObjectAdaptor,
                    public DBus::IntrospectableAdaptor {
 public:
  DebugDaemon(DBus::Connection* connection, DBus::BusDispatcher* dispatcher);
  ~DebugDaemon() override = default;

  bool Init();
  void Run();

  // Public methods below this point are part of the DBus interface presented by
  // this object, and are documented in </share/org.chromium.debugd.xml>.
  std::string BatteryFirmware(const std::string& option,
                              DBus::Error& error) override;  // NOLINT
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
  void GetPerfOutput(const uint32_t& duration_sec,
                     const std::vector<std::string>& perf_args,
                     int32_t& status,
                     std::vector<uint8_t>& perf_data,
                     std::vector<uint8_t>& perf_stat,
                     DBus::Error& error) override;  // NOLINT
  void GetPerfOutputFd(
      const uint32_t& duration_sec,
      const std::vector<std::string>& perf_args,
      const DBus::FileDescriptor& stdout_fd,
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
  void GetBigFeedbackLogs(const DBus::FileDescriptor& fd,
                          DBus::Error& error) override;  // NOLINT
  std::map<std::string, std::string> GetUserLogFiles(
      DBus::Error& error) override;  // NOLINT
  std::string GetExample(DBus::Error& error) override;  // NOLINT
  int32_t CupsAddAutoConfiguredPrinter(const std::string& name,
                                       const std::string& uri,
                                       DBus::Error& error) override;  // NOLINT
  int32_t CupsAddManuallyConfiguredPrinter(
      const std::string& name,
      const std::string& uri,
      const std::vector<uint8_t>& ppd_contents,
      DBus::Error& error) override;  // NOLINT
  bool CupsRemovePrinter(const std::string& name,
                         DBus::Error& error) override;  // NOLINT
  void CupsResetState(DBus::Error& error) override;  // NOLINT
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
  void RemoveRootfsVerification(DBus::Error& error) override;  // NOLINT
  void EnableBootFromUsb(DBus::Error& error) override;  // NOLINT
  void EnableChromeRemoteDebugging(DBus::Error& error) override;  // NOLINT
  void ConfigureSshServer(DBus::Error& error) override;  // NOLINT
  void SetUserPassword(const std::string& username,
                       const std::string& password,
                       DBus::Error& error) override;  // NOLINT
  void EnableChromeDevFeatures(const std::string& root_password,
                               DBus::Error& error) override;  // NOLINT
  int32_t QueryDevFeatures(DBus::Error& error) override;  // NOLINT
  void EnableDevCoredumpUpload(DBus::Error& error) override;  // NOLINT
  void DisableDevCoredumpUpload(DBus::Error& error) override;  // NOLINT
  std::string SetOomScoreAdj(const std::map<pid_t, int32_t>& scores,
                             DBus::Error& error) override;  // NOLINT
  std::string SwapEnable(const uint32_t& size, const bool& change_now,
                         DBus::Error& error) override;  // NOLINT
  std::string SwapDisable(const bool& change_now,
                          DBus::Error& error) override;  // NOLINT
  std::string SwapStartStop(const bool& on,
                            DBus::Error& error) override;  // NOLINT
  std::string SwapStatus(DBus::Error& error) override;  // NOLINT
  std::string SwapSetMargin(const uint32_t& size,
                            DBus::Error& error) override;  // NOLINT
  bool SetWifiDriverDebug(const int32_t& flags,
                          DBus::Error& error) override;  // NOLINT
  void ContainerStarted(DBus::Error& error) override;  // NOLINT
  void ContainerStopped(DBus::Error& error) override;  // NOLINT

 private:
  DBus::Connection* dbus_;
  DBus::BusDispatcher* dispatcher_;

  std::unique_ptr<SessionManagerProxy> session_manager_proxy_;
  std::unique_ptr<ContainerTool> container_tool_;
  std::unique_ptr<CrashSenderTool> crash_sender_tool_;
  std::unique_ptr<CupsTool> cups_tool_;
  std::unique_ptr<DebugLogsTool> debug_logs_tool_;
  std::unique_ptr<DebugModeTool> debug_mode_tool_;
  std::unique_ptr<RestrictedToolWrapper<DevFeaturesTool>>
      dev_features_tool_wrapper_;
  std::unique_ptr<ExampleTool> example_tool_;
  std::unique_ptr<ICMPTool> icmp_tool_;
  std::unique_ptr<LogTool> log_tool_;
  std::unique_ptr<MemtesterTool> memory_tool_;
  std::unique_ptr<ModemStatusTool> modem_status_tool_;
  std::unique_ptr<NetifTool> netif_tool_;
  std::unique_ptr<NetworkStatusTool> network_status_tool_;
  std::unique_ptr<OomAdjTool> oom_adj_tool_;
  std::unique_ptr<PacketCaptureTool> packet_capture_tool_;
  std::unique_ptr<PerfTool> perf_tool_;
  std::unique_ptr<PingTool> ping_tool_;
  std::unique_ptr<RouteTool> route_tool_;
  std::unique_ptr<StorageTool> storage_tool_;
  std::unique_ptr<SwapTool> swap_tool_;
  std::unique_ptr<BatteryTool> battery_tool_;
  std::unique_ptr<SysrqTool> sysrq_tool_;
  std::unique_ptr<SystraceTool> systrace_tool_;
  std::unique_ptr<TracePathTool> tracepath_tool_;
  std::unique_ptr<WifiDebugTool> wifi_debug_tool_;
  std::unique_ptr<WiMaxStatusTool> wimax_status_tool_;
};

}  // namespace debugd

#endif  // DEBUGD_SRC_DEBUG_DAEMON_H_
