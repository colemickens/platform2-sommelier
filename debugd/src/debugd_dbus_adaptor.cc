// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/debugd_dbus_adaptor.h"

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/memory/ptr_util.h>
#include <base/memory/ref_counted.h>
#include <brillo/variant_dictionary.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/object_path.h>

#include "debugd/src/constants.h"
#include "debugd/src/error_utils.h"

namespace debugd {

namespace {

const char kDevCoredumpDBusErrorString[] =
    "org.chromium.debugd.error.DevCoreDump";

}  // namespace

DebugdDBusAdaptor::DebugdDBusAdaptor(scoped_refptr<dbus::Bus> bus)
    : org::chromium::debugdAdaptor(this),
      dbus_object_(nullptr, bus, dbus::ObjectPath(kDebugdServicePath)) {
  battery_tool_ = base::MakeUnique<BatteryTool>();
  container_tool_ = base::MakeUnique<ContainerTool>();
  crash_sender_tool_ = base::MakeUnique<CrashSenderTool>();
  cups_tool_ = base::MakeUnique<CupsTool>();
  debug_logs_tool_ = base::MakeUnique<DebugLogsTool>();
  debug_mode_tool_ = base::MakeUnique<DebugModeTool>(bus);
  dev_features_tool_wrapper_ =
      base::MakeUnique<RestrictedToolWrapper<DevFeaturesTool>>(bus);
  example_tool_ = base::MakeUnique<ExampleTool>();
  icmp_tool_ = base::MakeUnique<ICMPTool>();
  log_tool_ = base::MakeUnique<LogTool>(bus);
  memory_tool_ = base::MakeUnique<MemtesterTool>();
  modem_status_tool_ = base::MakeUnique<ModemStatusTool>();
  netif_tool_ = base::MakeUnique<NetifTool>();
  network_status_tool_ = base::MakeUnique<NetworkStatusTool>();
  oom_adj_tool_ = base::MakeUnique<OomAdjTool>();
  packet_capture_tool_ = base::MakeUnique<PacketCaptureTool>();
  perf_tool_ = base::MakeUnique<PerfTool>();
  ping_tool_ = base::MakeUnique<PingTool>();
  route_tool_ = base::MakeUnique<RouteTool>();
  storage_tool_ = base::MakeUnique<StorageTool>();
  swap_tool_ = base::MakeUnique<SwapTool>();
  sysrq_tool_ = base::MakeUnique<SysrqTool>();
  systrace_tool_ = base::MakeUnique<SystraceTool>();
  tracepath_tool_ = base::MakeUnique<TracePathTool>();
  u2f_tool_ = base::MakeUnique<U2fTool>();
  wifi_debug_tool_ = base::MakeUnique<WifiDebugTool>();
  wifi_power_tool_ = base::MakeUnique<WifiPowerTool>();
  wimax_status_tool_ = base::MakeUnique<WiMaxStatusTool>();
  session_manager_proxy_ = base::MakeUnique<SessionManagerProxy>(bus);
  if (dev_features_tool_wrapper_->restriction().InDevMode() &&
      base::PathExists(
      base::FilePath(debugd::kDevFeaturesChromeRemoteDebuggingFlagPath))) {
    session_manager_proxy_->EnableChromeRemoteDebugging();
  }
}

void DebugdDBusAdaptor::RegisterAsync(
    const brillo::dbus_utils::AsyncEventSequencer::CompletionAction& cb) {
  RegisterWithDBusObject(&dbus_object_);
  dbus_object_.RegisterAsync(cb);
}

std::string DebugdDBusAdaptor::SetOomScoreAdj(
    const std::map<pid_t, int32_t>& scores) {
  return oom_adj_tool_->Set(scores);
}

bool DebugdDBusAdaptor::PingStart(brillo::ErrorPtr* error,
                                  const dbus::FileDescriptor& outfd,
                                  const std::string& destination,
                                  const brillo::VariantDictionary& options,
                                  std::string* handle) {
  return ping_tool_->Start(outfd, destination, options, handle, error);
}

bool DebugdDBusAdaptor::PingStop(brillo::ErrorPtr* error,
                                 const std::string& handle) {
  return ping_tool_->Stop(handle, error);
}

std::string DebugdDBusAdaptor::TracePathStart(
    const dbus::FileDescriptor& outfd,
    const std::string& destination,
    const brillo::VariantDictionary& options) {
  return tracepath_tool_->Start(outfd, destination, options);
}

bool DebugdDBusAdaptor::TracePathStop(brillo::ErrorPtr* error,
                                      const std::string& handle) {
  return tracepath_tool_->Stop(handle, error);
}

void DebugdDBusAdaptor::SystraceStart(const std::string& categories) {
  (void) systrace_tool_->Start(categories);
}

void DebugdDBusAdaptor::SystraceStop(const dbus::FileDescriptor& outfd) {
  systrace_tool_->Stop(outfd);
}

std::string DebugdDBusAdaptor::SystraceStatus() {
  return systrace_tool_->Status();
}

std::vector<std::string> DebugdDBusAdaptor::GetRoutes(
    const brillo::VariantDictionary& options) {
  return route_tool_->GetRoutes(options);
}

std::string DebugdDBusAdaptor::GetModemStatus() {
  return modem_status_tool_->GetModemStatus();
}

std::string DebugdDBusAdaptor::RunModemCommand(const std::string& command) {
  return modem_status_tool_->RunModemCommand(command);
}

std::string DebugdDBusAdaptor::GetNetworkStatus() {
  return network_status_tool_->GetNetworkStatus();
}

std::string DebugdDBusAdaptor::GetWiMaxStatus() {
  return wimax_status_tool_->GetWiMaxStatus();
}

bool DebugdDBusAdaptor::GetPerfOutput(
    brillo::ErrorPtr* error,
    uint32_t duration_sec,
    const std::vector<std::string>& perf_args,
    int32_t* status,
    std::vector<uint8_t>* perf_data,
    std::vector<uint8_t>* perf_stat) {
  return perf_tool_->GetPerfOutput(
      duration_sec, perf_args, perf_data, perf_stat, status, error);
}

bool DebugdDBusAdaptor::GetPerfOutputFd(
    brillo::ErrorPtr* error,
    uint32_t duration_sec,
    const std::vector<std::string>& perf_args,
    const dbus::FileDescriptor& stdout_fd) {
  return perf_tool_->GetPerfOutputFd(
      duration_sec, perf_args, stdout_fd, error);
}

void DebugdDBusAdaptor::DumpDebugLogs(bool is_compressed,
                                      const dbus::FileDescriptor& fd) {
  debug_logs_tool_->GetDebugLogs(is_compressed, fd);
}

void DebugdDBusAdaptor::SetDebugMode(const std::string& subsystem) {
  debug_mode_tool_->SetDebugMode(subsystem);
}

std::string DebugdDBusAdaptor::GetLog(const std::string& name) {
  return log_tool_->GetLog(name);
}

std::map<std::string, std::string> DebugdDBusAdaptor::GetAllLogs() {
  return log_tool_->GetAllLogs();
}

std::map<std::string, std::string> DebugdDBusAdaptor::GetFeedbackLogs() {
  return log_tool_->GetFeedbackLogs();
}

void DebugdDBusAdaptor::GetBigFeedbackLogs(const dbus::FileDescriptor& fd) {
  log_tool_->GetBigFeedbackLogs(fd);
}

std::map<std::string, std::string> DebugdDBusAdaptor::GetUserLogFiles() {
  return log_tool_->GetUserLogFiles();
}

std::string DebugdDBusAdaptor::GetExample() {
  return example_tool_->GetExample();
}

int32_t DebugdDBusAdaptor::CupsAddAutoConfiguredPrinter(
    const std::string& name,
    const std::string& uri) {
  return cups_tool_->AddAutoConfiguredPrinter(name, uri);
}

int32_t DebugdDBusAdaptor::CupsAddManuallyConfiguredPrinter(
    const std::string& name,
    const std::string& uri,
    const std::vector<uint8_t>& ppd_contents) {
  return cups_tool_->AddManuallyConfiguredPrinter(
      name, uri, ppd_contents);
}

bool DebugdDBusAdaptor::CupsRemovePrinter(const std::string& name) {
  return cups_tool_->RemovePrinter(name);
}

void DebugdDBusAdaptor::CupsResetState() {
  cups_tool_->ResetState();
}

std::string DebugdDBusAdaptor::GetInterfaces() {
  return netif_tool_->GetInterfaces();
}

std::string DebugdDBusAdaptor::TestICMP(const std::string& host) {
  return icmp_tool_->TestICMP(host);
}

std::string DebugdDBusAdaptor::TestICMPWithOptions(
    const std::string& host,
    const std::map<std::string, std::string>& options) {
  return icmp_tool_->TestICMPWithOptions(host, options);
}

std::string DebugdDBusAdaptor::BatteryFirmware(const std::string& option) {
  return battery_tool_->BatteryFirmware(option);
}

std::string DebugdDBusAdaptor::Smartctl(const std::string& option) {
  return storage_tool_->Smartctl(option);
}

std::string DebugdDBusAdaptor::MemtesterStart(const dbus::FileDescriptor& outfd,
                                              uint32_t memory) {
  return memory_tool_->Start(outfd, memory);
}

bool DebugdDBusAdaptor::MemtesterStop(brillo::ErrorPtr* error,
                                      const std::string& handle) {
  return memory_tool_->Stop(handle, error);
}

std::string DebugdDBusAdaptor::BadblocksStart(
    const dbus::FileDescriptor& outfd) {
  return storage_tool_->Start(outfd);
}

bool DebugdDBusAdaptor::BadblocksStop(brillo::ErrorPtr* error,
                                      const std::string& handle) {
  return storage_tool_->Stop(handle, error);
}

bool DebugdDBusAdaptor::PacketCaptureStart(
    brillo::ErrorPtr* error,
    const dbus::FileDescriptor& statfd,
    const dbus::FileDescriptor& outfd,
    const brillo::VariantDictionary& options,
    std::string* handle) {
  return packet_capture_tool_->Start(statfd, outfd, options, handle, error);
}

bool DebugdDBusAdaptor::PacketCaptureStop(brillo::ErrorPtr* error,
                                          const std::string& handle) {
  return packet_capture_tool_->Stop(handle, error);
}

bool DebugdDBusAdaptor::LogKernelTaskStates(brillo::ErrorPtr* error) {
  return sysrq_tool_->LogKernelTaskStates(error);
}

void DebugdDBusAdaptor::UploadCrashes() {
  crash_sender_tool_->UploadCrashes();
}

bool DebugdDBusAdaptor::RemoveRootfsVerification(brillo::ErrorPtr* error) {
  auto tool = dev_features_tool_wrapper_->GetTool(error);
  return tool && tool->RemoveRootfsVerification(error);
}

bool DebugdDBusAdaptor::EnableBootFromUsb(brillo::ErrorPtr* error) {
  auto tool = dev_features_tool_wrapper_->GetTool(error);
  return tool && tool->EnableBootFromUsb(error);
}

bool DebugdDBusAdaptor::EnableChromeRemoteDebugging(brillo::ErrorPtr* error) {
  auto tool = dev_features_tool_wrapper_->GetTool(error);
  return tool && tool->EnableChromeRemoteDebugging(error);
}

bool DebugdDBusAdaptor::ConfigureSshServer(brillo::ErrorPtr* error) {
  auto tool = dev_features_tool_wrapper_->GetTool(error);
  return tool && tool->ConfigureSshServer(error);
}

bool DebugdDBusAdaptor::SetUserPassword(brillo::ErrorPtr* error,
                                        const std::string& username,
                                        const std::string& password) {
  auto tool = dev_features_tool_wrapper_->GetTool(error);
  return tool && tool->SetUserPassword(username, password, error);
}

bool DebugdDBusAdaptor::EnableChromeDevFeatures(
    brillo::ErrorPtr* error,
    const std::string& root_password) {
  auto tool = dev_features_tool_wrapper_->GetTool(error);
  return tool && tool->EnableChromeDevFeatures(root_password, error);
}

bool DebugdDBusAdaptor::QueryDevFeatures(brillo::ErrorPtr* error,
                                         int32_t* features) {
  // Special case: if access fails here, we return DEV_FEATURES_DISABLED rather
  // than a D-Bus error. However, we still want to return an error if we can
  // access the tool but the tool execution fails.
  auto tool = dev_features_tool_wrapper_->GetTool(nullptr);
  if (!tool) {
    *features = DEV_FEATURES_DISABLED;
    return true;
  }

  return tool && tool->QueryDevFeatures(features, error);
}

bool DebugdDBusAdaptor::EnableDevCoredumpUpload(brillo::ErrorPtr* error) {
  if (base::PathExists(
      base::FilePath(debugd::kDeviceCoredumpUploadFlagPath))) {
    VLOG(1) << "Device coredump upload already enabled";
    return false;
  }
  if (base::WriteFile(
      base::FilePath(debugd::kDeviceCoredumpUploadFlagPath), "", 0) < 0) {
    DEBUGD_ADD_ERROR(error,
              kDevCoredumpDBusErrorString,
              "Failed to write flag file.");
    PLOG(ERROR) << "Failed to write flag file.";
    return false;
  }
  return true;
}

bool DebugdDBusAdaptor::DisableDevCoredumpUpload(brillo::ErrorPtr* error) {
  if (!base::PathExists(
      base::FilePath(debugd::kDeviceCoredumpUploadFlagPath))) {
    VLOG(1) << "Device coredump upload already disabled";
    return false;
  }
  if (!base::DeleteFile(
      base::FilePath(debugd::kDeviceCoredumpUploadFlagPath), false)) {
    DEBUGD_ADD_ERROR(error,
              kDevCoredumpDBusErrorString,
              "Failed to delete flag file.");
    PLOG(ERROR) << "Failed to delete flag file.";
    return false;
  }
  return true;
}

std::string DebugdDBusAdaptor::SwapEnable(uint32_t size, bool change_now) {
  return swap_tool_->SwapEnable(size, change_now);
}

std::string DebugdDBusAdaptor::SwapDisable(bool change_now) {
  return swap_tool_->SwapDisable(change_now);
}

std::string DebugdDBusAdaptor::SwapStartStop(bool on) {
  return swap_tool_->SwapStartStop(on);
}

std::string DebugdDBusAdaptor::SwapStatus() {
  return swap_tool_->SwapStatus();
}

std::string DebugdDBusAdaptor::SwapSetParameter(
    const std::string& parameter_name,
    uint32_t parameter_value) {
  return swap_tool_->SwapSetParameter(parameter_name, parameter_value);
}

std::string DebugdDBusAdaptor::SetU2fFlags(const std::string& flags) {
  return u2f_tool_->SetFlags(flags);
}

bool DebugdDBusAdaptor::SetWifiDriverDebug(int32_t flags) {
  return wifi_debug_tool_->SetEnabled(debugd::WifiDebugFlag(flags), nullptr);
}

void DebugdDBusAdaptor::ContainerStarted() {
  container_tool_->ContainerStarted();
}

void DebugdDBusAdaptor::ContainerStopped() {
  container_tool_->ContainerStopped();
}

std::string DebugdDBusAdaptor::SetWifiPowerSave(bool enable) {
  return wifi_power_tool_->SetWifiPowerSave(enable);
}

std::string DebugdDBusAdaptor::GetWifiPowerSave() {
  return wifi_power_tool_->GetWifiPowerSave();
}

}  // namespace debugd
