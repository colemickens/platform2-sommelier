// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/debug_daemon.h"

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/memory/ptr_util.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus-c++/dbus.h>

#include "debugd/src/constants.h"

namespace debugd {

namespace {

const char kDevCoredumpDBusErrorString[] =
    "org.chromium.debugd.error.DevCoreDump";

}  // namespace

DebugDaemon::DebugDaemon(DBus::Connection* connection,
                         DBus::BusDispatcher* dispatcher)
    : DBus::ObjectAdaptor(*connection, kDebugdServicePath),
      dbus_(connection),
      dispatcher_(dispatcher) {}

bool DebugDaemon::Init() {
  battery_tool_ = base::MakeUnique<BatteryTool>();
  container_tool_ = base::MakeUnique<ContainerTool>();
  crash_sender_tool_ = base::MakeUnique<CrashSenderTool>();
  cups_tool_ = base::MakeUnique<CupsTool>();
  debug_logs_tool_ = base::MakeUnique<DebugLogsTool>();
  debug_mode_tool_ = base::MakeUnique<DebugModeTool>(dbus_);
  dev_features_tool_wrapper_ =
      base::MakeUnique<RestrictedToolWrapper<DevFeaturesTool>>(dbus_);
  example_tool_ = base::MakeUnique<ExampleTool>();
  icmp_tool_ = base::MakeUnique<ICMPTool>();
  modem_status_tool_ = base::MakeUnique<ModemStatusTool>();
  netif_tool_ = base::MakeUnique<NetifTool>();
  network_status_tool_ = base::MakeUnique<NetworkStatusTool>();
  oom_adj_tool_ = base::MakeUnique<OomAdjTool>();
  packet_capture_tool_ = base::MakeUnique<PacketCaptureTool>();
  ping_tool_ = base::MakeUnique<PingTool>();
  route_tool_ = base::MakeUnique<RouteTool>();
  sysrq_tool_ = base::MakeUnique<SysrqTool>();
  systrace_tool_ = base::MakeUnique<SystraceTool>();
  tracepath_tool_ = base::MakeUnique<TracePathTool>();
  log_tool_ = base::MakeUnique<LogTool>();
  perf_tool_ = base::MakeUnique<PerfTool>();
  storage_tool_ = base::MakeUnique<StorageTool>();
  swap_tool_ = base::MakeUnique<SwapTool>();
  memory_tool_ = base::MakeUnique<MemtesterTool>();
  wifi_debug_tool_ = base::MakeUnique<WifiDebugTool>();
  wimax_status_tool_ = base::MakeUnique<WiMaxStatusTool>();
  if (!dbus_->acquire_name(kDebugdServiceName)) {
    LOG(ERROR) << "Failed to acquire D-Bus name " << kDebugdServiceName;
    return false;
  }
  session_manager_proxy_ = base::MakeUnique<SessionManagerProxy>(dbus_);
  if (dev_features_tool_wrapper_->restriction().InDevMode() &&
      base::PathExists(
      base::FilePath(debugd::kDevFeaturesChromeRemoteDebuggingFlagPath))) {
    session_manager_proxy_->EnableChromeRemoteDebugging();
  }
  return true;
}

void DebugDaemon::Run() {
  dispatcher_->enter();
  while (1) {
    dispatcher_->do_iteration();
  }
  // Unreachable.
  dispatcher_->leave();
}

std::string DebugDaemon::SetOomScoreAdj(const std::map<pid_t, int32_t>& scores,
                                        DBus::Error& error) {
  return oom_adj_tool_->Set(scores);
}

std::string DebugDaemon::PingStart(
    const DBus::FileDescriptor& outfd,
    const std::string& destination,
    const std::map<std::string, DBus::Variant>& options,
    DBus::Error& error) {  // NOLINT
  std::string id;
  if (!ping_tool_->Start(outfd, destination, options, &id, &error))
    return "";

  return id;
}

void DebugDaemon::PingStop(const std::string& handle,
                           DBus::Error& error) {  // NOLINT
  ping_tool_->Stop(handle, &error);
}

std::string DebugDaemon::TracePathStart(
    const DBus::FileDescriptor& outfd,
    const std::string& destination,
    const std::map<std::string, DBus::Variant>& options,
    DBus::Error& error) {  // NOLINT
  return tracepath_tool_->Start(outfd, destination, options);
}

void DebugDaemon::TracePathStop(const std::string& handle,
                                DBus::Error& error) {  // NOLINT
  tracepath_tool_->Stop(handle, &error);
}

void DebugDaemon::SystraceStart(const std::string& categories,
                                DBus::Error& error) {  // NOLINT
  (void) systrace_tool_->Start(categories);
}

void DebugDaemon::SystraceStop(const DBus::FileDescriptor& outfd,
                               DBus::Error& error) { // NOLINT
  systrace_tool_->Stop(outfd);
}

std::string DebugDaemon::SystraceStatus(DBus::Error& error) {  // NOLINT
  return systrace_tool_->Status();
}

std::vector<std::string> DebugDaemon::GetRoutes(
    const std::map<std::string, DBus::Variant>& options,
    DBus::Error& error) {  // NOLINT
  return route_tool_->GetRoutes(options);
}

std::string DebugDaemon::GetModemStatus(DBus::Error& error) {  // NOLINT
  return modem_status_tool_->GetModemStatus();
}

std::string DebugDaemon::RunModemCommand(const std::string& command,
                                         DBus::Error& error) {  // NOLINT
  return modem_status_tool_->RunModemCommand(command);
}

std::string DebugDaemon::GetNetworkStatus(DBus::Error& error) { // NOLINT
  return network_status_tool_->GetNetworkStatus();
}

std::string DebugDaemon::GetWiMaxStatus(DBus::Error& error) { // NOLINT
  return wimax_status_tool_->GetWiMaxStatus();
}

void DebugDaemon::GetPerfOutput(
    const uint32_t& duration_sec,
    const std::vector<std::string>& perf_args,
    int32_t& status,
    std::vector<uint8_t>& perf_data,
    std::vector<uint8_t>& perf_stat,
    DBus::Error& error) {  // NOLINT
  status = perf_tool_->GetPerfOutput(
      duration_sec, perf_args, &perf_data, &perf_stat, &error);
}

void DebugDaemon::GetPerfOutputFd(
    const uint32_t& duration_sec,
    const std::vector<std::string>& perf_args,
    const DBus::FileDescriptor& stdout_fd,
    DBus::Error& error) {  // NOLINT
  perf_tool_->GetPerfOutputFd(
      duration_sec, perf_args, stdout_fd, &error);
}

void DebugDaemon::DumpDebugLogs(const bool& is_compressed,
                                const DBus::FileDescriptor& fd,
                                DBus::Error& error) {  // NOLINT
  debug_logs_tool_->GetDebugLogs(is_compressed, fd);
}

void DebugDaemon::SetDebugMode(const std::string& subsystem,
                               DBus::Error& error) {  // NOLINT
  debug_mode_tool_->SetDebugMode(subsystem);
}

std::string DebugDaemon::GetLog(const std::string& name,
                                DBus::Error& error) {  // NOLINT
  return log_tool_->GetLog(name);
}

std::map<std::string, std::string> DebugDaemon::GetAllLogs(
    DBus::Error& error) {  // NOLINT
  return log_tool_->GetAllLogs(dbus_);
}

std::map<std::string, std::string> DebugDaemon::GetFeedbackLogs(
    DBus::Error& error) {  // NOLINT
  return log_tool_->GetFeedbackLogs(dbus_);
}

void DebugDaemon::GetBigFeedbackLogs(const DBus::FileDescriptor& fd,
                                     DBus::Error& error) {  // NOLINT
  log_tool_->GetBigFeedbackLogs(dbus_, fd);
}

std::map<std::string, std::string> DebugDaemon::GetUserLogFiles(
    DBus::Error& error) {  // NOLINT
  return log_tool_->GetUserLogFiles();
}

std::string DebugDaemon::GetExample(DBus::Error& error) {  // NOLINT
  return example_tool_->GetExample();
}

int32_t DebugDaemon::CupsAddAutoConfiguredPrinter(
    const std::string& name,
    const std::string& uri,
    DBus::Error& error) {  // NOLINT
  return cups_tool_->AddAutoConfiguredPrinter(name, uri);
}

int32_t DebugDaemon::CupsAddManuallyConfiguredPrinter(
    const std::string& name,
    const std::string& uri,
    const std::vector<uint8_t>& ppd_contents,
    DBus::Error& error) {  // NOLINT
  return cups_tool_->AddManuallyConfiguredPrinter(
      name, uri, ppd_contents);
}

bool DebugDaemon::CupsRemovePrinter(const std::string& name,
                                    DBus::Error& error) { // NOLINT
  return cups_tool_->RemovePrinter(name);
}

void DebugDaemon::CupsResetState(DBus::Error& error) { // NOLINT
  cups_tool_->ResetState();
}

std::string DebugDaemon::GetInterfaces(DBus::Error& error) {  // NOLINT
  return netif_tool_->GetInterfaces();
}

std::string DebugDaemon::TestICMP(const std::string& host,
                                  DBus::Error& error) {  // NOLINT
  return icmp_tool_->TestICMP(host);
}

std::string DebugDaemon::TestICMPWithOptions(
    const std::string& host,
    const std::map<std::string, std::string>& options,
    DBus::Error& error) {  // NOLINT
  return icmp_tool_->TestICMPWithOptions(host, options);
}

std::string DebugDaemon::BatteryFirmware(const std::string& option,
                                         DBus::Error& error) {  // NOLINT
  return battery_tool_->BatteryFirmware(option);
}

std::string DebugDaemon::Smartctl(const std::string& option,
                                  DBus::Error& error) {  // NOLINT
  return storage_tool_->Smartctl(option);
}

std::string DebugDaemon::MemtesterStart(const DBus::FileDescriptor& outfd,
                                        const uint32_t& memory,
                                        DBus::Error& error) {  // NOLINT
  return memory_tool_->Start(outfd, memory);
}

void DebugDaemon::MemtesterStop(const std::string& handle,
                                DBus::Error& error) {  // NOLINT
  memory_tool_->Stop(handle, &error);
}

std::string DebugDaemon::BadblocksStart(const DBus::FileDescriptor& outfd,
                                        DBus::Error& error) {  // NOLINT
  return storage_tool_->Start(outfd);
}

void DebugDaemon::BadblocksStop(const std::string& handle,
                                DBus::Error& error) {  // NOLINT
  storage_tool_->Stop(handle, &error);
}

std::string DebugDaemon::PacketCaptureStart(
    const DBus::FileDescriptor& statfd,
    const DBus::FileDescriptor& outfd,
    const std::map<std::string, DBus::Variant>& options,
    DBus::Error& error) {  // NOLINT
  std::string id;
  if (!packet_capture_tool_->Start(statfd, outfd, options, &id, &error))
    return "";

  return id;
}

void DebugDaemon::PacketCaptureStop(const std::string& handle,
                                    DBus::Error& error) {  // NOLINT
  packet_capture_tool_->Stop(handle, &error);
}

void DebugDaemon::LogKernelTaskStates(DBus::Error& error) {  // NOLINT
  sysrq_tool_->LogKernelTaskStates(&error);
}

void DebugDaemon::UploadCrashes(DBus::Error& error) {  // NOLINT
  crash_sender_tool_->UploadCrashes();
}

void DebugDaemon::RemoveRootfsVerification(DBus::Error& error) {  // NOLINT
  auto tool = dev_features_tool_wrapper_->GetTool(&error);
  if (tool)
    tool->RemoveRootfsVerification(&error);
}

void DebugDaemon::EnableBootFromUsb(DBus::Error& error) {  // NOLINT
  auto tool = dev_features_tool_wrapper_->GetTool(&error);
  if (tool)
    tool->EnableBootFromUsb(&error);
}

void DebugDaemon::EnableChromeRemoteDebugging(DBus::Error& error) {  // NOLINT
  auto tool = dev_features_tool_wrapper_->GetTool(&error);
  if (tool)
    tool->EnableChromeRemoteDebugging(&error);
}

void DebugDaemon::ConfigureSshServer(DBus::Error& error) {  // NOLINT
  auto tool = dev_features_tool_wrapper_->GetTool(&error);
  if (tool)
    tool->ConfigureSshServer(&error);
}

void DebugDaemon::SetUserPassword(const std::string& username,
                                  const std::string& password,
                                  DBus::Error& error) {  // NOLINT
  auto tool = dev_features_tool_wrapper_->GetTool(&error);
  if (tool)
    tool->SetUserPassword(username, password, &error);
}

void DebugDaemon::EnableChromeDevFeatures(const std::string& root_password,
                                          DBus::Error& error) {  // NOLINT
  auto tool = dev_features_tool_wrapper_->GetTool(nullptr);
  if (tool)
    tool->EnableChromeDevFeatures(root_password, &error);
}

int32_t DebugDaemon::QueryDevFeatures(DBus::Error& error) {  // NOLINT
  // Special case: if access fails here, we return DEV_FEATURES_DISABLED rather
  // than a D-Bus error. However, we still want to return an error if we can
  // access the tool but the tool execution fails.
  auto tool = dev_features_tool_wrapper_->GetTool(nullptr);
  if (!tool)
    return DEV_FEATURES_DISABLED;

  int32_t features;
  if (!tool->QueryDevFeatures(&features, &error))
    return DEV_FEATURES_DISABLED;

  return features;
}

void DebugDaemon::EnableDevCoredumpUpload(DBus::Error& error) {  // NOLINT
  if (base::PathExists(
      base::FilePath(debugd::kDeviceCoredumpUploadFlagPath))) {
    VLOG(1) << "Device coredump upload already enabled";
    return;
  }
  if (base::WriteFile(
      base::FilePath(debugd::kDeviceCoredumpUploadFlagPath), "", 0) < 0) {
    error.set(kDevCoredumpDBusErrorString, "Failed to write flag file.");
    PLOG(ERROR) << "Failed to write flag file.";
    return;
  }
}

void DebugDaemon::DisableDevCoredumpUpload(DBus::Error& error) {  // NOLINT
  if (!base::PathExists(
      base::FilePath(debugd::kDeviceCoredumpUploadFlagPath))) {
    VLOG(1) << "Device coredump upload already disabled";
    return;
  }
  if (!base::DeleteFile(
      base::FilePath(debugd::kDeviceCoredumpUploadFlagPath), false)) {
    error.set(kDevCoredumpDBusErrorString, "Failed to delete flag file.");
    PLOG(ERROR) << "Failed to delete flag file.";
    return;
  }
}

std::string DebugDaemon::SwapEnable(const uint32_t& size,
                                    const bool& change_now,
                                    DBus::Error& error) {  // NOLINT
  return swap_tool_->SwapEnable(size, change_now);
}

std::string DebugDaemon::SwapDisable(const bool& change_now,
                                     DBus::Error& error) {  // NOLINT
  return swap_tool_->SwapDisable(change_now);
}

std::string DebugDaemon::SwapStartStop(const bool& on,
                                       DBus::Error& error) {  // NOLINT
  return swap_tool_->SwapStartStop(on);
}

std::string DebugDaemon::SwapStatus(DBus::Error& error) {  // NOLINT
  return swap_tool_->SwapStatus();
}

std::string DebugDaemon::SwapSetParameter(const std::string& parameter_name,
                                          const uint32_t& parameter_value,
                                          DBus::Error& error) {  // NOLINT
  return swap_tool_->SwapSetParameter(parameter_name, parameter_value);
}

bool DebugDaemon::SetWifiDriverDebug(const int32_t& flags,
                                     DBus::Error& error) {  // NOLINT
  return wifi_debug_tool_->SetEnabled(debugd::WifiDebugFlag(flags), &error);
}

void DebugDaemon::ContainerStarted(DBus::Error& error) {  // NOLINT
  container_tool_->ContainerStarted();
}

void DebugDaemon::ContainerStopped(DBus::Error& error) {  // NOLINT
  container_tool_->ContainerStopped();
}

}  // namespace debugd
