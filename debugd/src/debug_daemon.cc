// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/debug_daemon.h"

#include <base/logging.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus-c++/dbus.h>

namespace debugd {

DebugDaemon::DebugDaemon(DBus::Connection* connection,
                         DBus::BusDispatcher* dispatcher)
    : DBus::ObjectAdaptor(*connection, kDebugdServicePath),
      dbus_(connection),
      dispatcher_(dispatcher) {}

bool DebugDaemon::Init() {
  crash_sender_tool_ = new CrashSenderTool();
  debug_logs_tool_ = new DebugLogsTool();
  debug_mode_tool_ = new DebugModeTool(dbus_);
  example_tool_ = new ExampleTool();
  icmp_tool_ = new ICMPTool();
  modem_status_tool_ = new ModemStatusTool();
  netif_tool_ = new NetifTool();
  network_status_tool_ = new NetworkStatusTool();
  packet_capture_tool_ = new PacketCaptureTool();
  ping_tool_ = new PingTool();
  route_tool_ = new RouteTool();
  sysrq_tool_ = new SysrqTool();
  systrace_tool_ = new SystraceTool();
  tracepath_tool_ = new TracePathTool();
  log_tool_ = new LogTool();
  perf_tool_ = new PerfTool();
  storage_tool_ = new StorageTool();
  memory_tool_ = new MemtesterTool();
  wimax_status_tool_ = new WiMaxStatusTool();
  if (!dbus_->acquire_name(kDebugdServiceName)) {
    LOG(ERROR) << "Failed to acquire D-Bus name " << kDebugdServiceName;
    return false;
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

std::string DebugDaemon::PingStart(
    const DBus::FileDescriptor& outfd,
    const std::string& destination,
    const std::map<std::string, DBus::Variant>& options,
    DBus::Error& error) {  // NOLINT
  return ping_tool_->Start(outfd, destination, options, &error);
}

void DebugDaemon::PingStop(const std::string& handle,
                           DBus::Error& error) {  // NOLINT
  return ping_tool_->Stop(handle, &error);
}

std::string DebugDaemon::TracePathStart(
    const DBus::FileDescriptor& outfd,
    const std::string& destination,
    const std::map<std::string, DBus::Variant>& options,
    DBus::Error& error) {  // NOLINT
  return tracepath_tool_->Start(outfd, destination, options, &error);
}

void DebugDaemon::TracePathStop(const std::string& handle,
                                DBus::Error& error) {  // NOLINT
  return tracepath_tool_->Stop(handle, &error);
}

void DebugDaemon::SystraceStart(const std::string& categories,
                                DBus::Error& error) {  // NOLINT
  (void) systrace_tool_->Start(categories, &error);
}

void DebugDaemon::SystraceStop(const DBus::FileDescriptor& outfd,
                               DBus::Error& error) { // NOLINT
  return systrace_tool_->Stop(outfd, &error);
}

std::string DebugDaemon::SystraceStatus(DBus::Error& error) {  // NOLINT
  return systrace_tool_->Status(&error);
}

std::vector<std::string> DebugDaemon::GetRoutes(
    const std::map<std::string, DBus::Variant>& options,
    DBus::Error& error) {  // NOLINT
  return route_tool_->GetRoutes(options, &error);
}

std::string DebugDaemon::GetModemStatus(DBus::Error& error) {  // NOLINT
  return modem_status_tool_->GetModemStatus(&error);
}

std::string DebugDaemon::RunModemCommand(const std::string& command,
                                         DBus::Error& error) {  // NOLINT
  return modem_status_tool_->RunModemCommand(command);
}

std::string DebugDaemon::GetNetworkStatus(DBus::Error& error) { // NOLINT
  return network_status_tool_->GetNetworkStatus(&error);
}

std::string DebugDaemon::GetWiMaxStatus(DBus::Error& error) { // NOLINT
  return wimax_status_tool_->GetWiMaxStatus(&error);
}

std::vector<uint8_t> DebugDaemon::GetRichPerfData(
    const uint32_t& duration, DBus::Error& error) {  // NOLINT
  return perf_tool_->GetRichPerfData(duration, &error);
}

void DebugDaemon::GetDebugLogs(const DBus::FileDescriptor& fd,
                               DBus::Error& error) {  // NOLINT
  debug_logs_tool_->GetDebugLogs(true,  // is_compressed,
                                 fd,
                                 &error);
}

void DebugDaemon::DumpDebugLogs(const bool& is_compressed,
                                const DBus::FileDescriptor& fd,
                                DBus::Error& error) {  // NOLINT
  debug_logs_tool_->GetDebugLogs(is_compressed, fd, &error);
}

void DebugDaemon::SetDebugMode(const std::string& subsystem,
                               DBus::Error& error) {  // NOLINT
  debug_mode_tool_->SetDebugMode(subsystem, &error);
}

std::string DebugDaemon::GetLog(const std::string& name,
                                DBus::Error& error) {  // NOLINT
  return log_tool_->GetLog(name, &error);
}

std::map<std::string, std::string> DebugDaemon::GetAllLogs(
    DBus::Error& error) {  // NOLINT
  return log_tool_->GetAllLogs(dbus_, &error);
}

std::map<std::string, std::string> DebugDaemon::GetFeedbackLogs(
    DBus::Error& error) {  // NOLINT
  return log_tool_->GetFeedbackLogs(dbus_, &error);
}

std::map<std::string, std::string> DebugDaemon::GetUserLogFiles(
    DBus::Error& error) {  // NOLINT
  return log_tool_->GetUserLogFiles(&error);
}

std::string DebugDaemon::GetExample(DBus::Error& error) {  // NOLINT
  return example_tool_->GetExample(&error);
}

std::string DebugDaemon::GetInterfaces(DBus::Error& error) {  // NOLINT
  return netif_tool_->GetInterfaces(&error);
}

std::string DebugDaemon::TestICMP(const std::string& host,
                                  DBus::Error& error) {  // NOLINT
  return icmp_tool_->TestICMP(host, &error);
}

std::string DebugDaemon::TestICMPWithOptions(
    const std::string& host,
    const std::map<std::string, std::string>& options,
    DBus::Error& error) {  // NOLINT
  return icmp_tool_->TestICMPWithOptions(host, options, &error);
}

std::string DebugDaemon::Smartctl(const std::string& option,
                                  DBus::Error& error) {  // NOLINT
  return storage_tool_->Smartctl(option, &error);
}

std::string DebugDaemon::MemtesterStart(const DBus::FileDescriptor& outfd,
                                        const uint32_t& memory,
                                        DBus::Error& error) {  // NOLINT
  return memory_tool_->Start(outfd, memory, &error);
}

void DebugDaemon::MemtesterStop(const std::string& handle,
                                DBus::Error& error) {  // NOLINT
  return memory_tool_->Stop(handle, &error);
}

std::string DebugDaemon::BadblocksStart(const DBus::FileDescriptor& outfd,
                                        DBus::Error& error) {  // NOLINT
  return storage_tool_->Start(outfd, &error);
}

void DebugDaemon::BadblocksStop(const std::string& handle,
                                DBus::Error& error) {  // NOLINT
  return storage_tool_->Stop(handle, &error);
}

std::string DebugDaemon::PacketCaptureStart(
    const DBus::FileDescriptor& statfd,
    const DBus::FileDescriptor& outfd,
    const std::map<std::string, DBus::Variant>& options,
    DBus::Error& error) {  // NOLINT
  return packet_capture_tool_->Start(statfd, outfd, options, &error);
}

void DebugDaemon::PacketCaptureStop(const std::string& handle,
                                    DBus::Error& error) {  // NOLINT
  return packet_capture_tool_->Stop(handle, &error);
}

void DebugDaemon::LogKernelTaskStates(DBus::Error& error) {  // NOLINT
  sysrq_tool_->LogKernelTaskStates(&error);
}

void DebugDaemon::UploadCrashes(DBus::Error& error) {  // NOLINT
  crash_sender_tool_->UploadCrashes(&error);
}

}  // namespace debugd
