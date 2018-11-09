// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apmanager/service.h"

#include <signal.h>

#include <base/bind.h>
#include <base/strings/stringprintf.h>
#include <brillo/errors/error.h>
#include <chromeos/dbus/service_constants.h>

#include "apmanager/control_interface.h"
#include "apmanager/manager.h"

using std::string;

namespace apmanager {

// static.
const char Service::kHostapdPath[] = "/usr/sbin/hostapd";
const char Service::kHostapdConfigPathFormat[] =
    "/run/apmanager/hostapd/hostapd-%d.conf";
const char Service::kHostapdControlInterfacePath[] =
    "/run/apmanager/hostapd/ctrl_iface";

const int Service::kTerminationTimeoutSeconds = 2;

// static. Service state definitions.
const char Service::kStateIdle[] = "Idle";
const char Service::kStateStarting[] = "Starting";
const char Service::kStateStarted[] = "Started";
const char Service::kStateFailed[] = "Failed";

Service::Service(Manager* manager, int service_identifier)
    : manager_(manager),
      identifier_(service_identifier),
      config_(new Config(manager, service_identifier)),
      adaptor_(manager->control_interface()->CreateServiceAdaptor(this)),
      dhcp_server_factory_(DHCPServerFactory::GetInstance()),
      file_writer_(FileWriter::GetInstance()),
      process_factory_(ProcessFactory::GetInstance()) {
  adaptor_->SetConfig(config_.get());
  adaptor_->SetState(kStateIdle);
  // TODO(zqiu): come up with better server address management. This is good
  // enough for now.
  config_->SetServerAddressIndex(identifier_ & 0xFF);
}

Service::~Service() {
  // Stop hostapd process if still running.
  if (IsHostapdRunning()) {
    ReleaseResources();
  }
}

bool Service::StartInternal(Error* error) {
  if (IsHostapdRunning()) {
    Error::PopulateAndLog(
        error, Error::kInternalError, "Service already running", FROM_HERE);
    return false;
  }

  // Setup hostapd control interface path.
  config_->set_control_interface(kHostapdControlInterfacePath);

  // Generate hostapd configuration content.
  string config_str;
  if (!config_->GenerateConfigFile(error, &config_str)) {
    return false;
  }

  // Write configuration to a file.
  string config_file_name = base::StringPrintf(kHostapdConfigPathFormat,
                                               identifier_);
  if (!file_writer_->Write(config_file_name, config_str)) {
    Error::PopulateAndLog(error,
                          Error::kInternalError,
                          "Failed to write configuration to a file",
                          FROM_HERE);
    return false;
  }

  // Claim the device needed for this ap service.
  if (!config_->ClaimDevice()) {
    Error::PopulateAndLog(error,
                          Error::kInternalError,
                          "Failed to claim the device for this service",
                          FROM_HERE);
    return false;
  }

  // Start hostapd process.
  if (!StartHostapdProcess(config_file_name)) {
    Error::PopulateAndLog(
        error, Error::kInternalError, "Failed to start hostapd", FROM_HERE);
    // Release the device claimed for this service.
    config_->ReleaseDevice();
    return false;
  }

  // Start DHCP server if in server mode.
  if (config_->GetOperationMode() == kOperationModeServer) {
    dhcp_server_.reset(
        dhcp_server_factory_->CreateDHCPServer(config_->GetServerAddressIndex(),
                                               config_->selected_interface()));
    if (!dhcp_server_->Start()) {
      Error::PopulateAndLog(error,
                            Error::kInternalError,
                            "Failed to start DHCP server",
                            FROM_HERE);
      ReleaseResources();
      return false;
    }
    manager_->RequestDHCPPortAccess(config_->selected_interface());
  }

  // Start monitoring hostapd.
  if (!hostapd_monitor_) {
    hostapd_monitor_.reset(
        new HostapdMonitor(base::Bind(&Service::HostapdEventCallback,
                                      weak_factory_.GetWeakPtr()),
                           config_->control_interface(),
                           config_->selected_interface()));
  }
  hostapd_monitor_->Start();

  // Update service state.
  adaptor_->SetState(kStateStarting);

  return true;
}

void Service::Start(const base::Callback<void(const Error&)>& result_callback) {
  Error error;
  StartInternal(&error);
  result_callback.Run(error);
}

bool Service::Stop(Error* error) {
  if (!IsHostapdRunning()) {
    Error::PopulateAndLog(error,
                          Error::kInternalError,
                          "Service is not currently running", FROM_HERE);
    return false;
  }

  ReleaseResources();
  adaptor_->SetState(kStateIdle);
  return true;
}

bool Service::IsHostapdRunning() {
  return hostapd_process_ && hostapd_process_->pid() != 0 &&
         brillo::Process::ProcessExists(hostapd_process_->pid());
}

bool Service::StartHostapdProcess(const string& config_file_path) {
  hostapd_process_.reset(process_factory_->CreateProcess());
  hostapd_process_->AddArg(kHostapdPath);
  hostapd_process_->AddArg(config_file_path);
  if (!hostapd_process_->Start()) {
    hostapd_process_.reset();
    return false;
  }
  return true;
}

void Service::StopHostapdProcess() {
  if (!hostapd_process_->Kill(SIGTERM, kTerminationTimeoutSeconds)) {
    hostapd_process_->Kill(SIGKILL, kTerminationTimeoutSeconds);
  }
  hostapd_process_.reset();
}

void Service::ReleaseResources() {
  hostapd_monitor_.reset();
  StopHostapdProcess();
  dhcp_server_.reset();
  manager_->ReleaseDHCPPortAccess(config_->selected_interface());
  // Only release device after mode switching had completed, to
  // make sure the station mode interface gets enumerated by
  // shill.
  config_->ReleaseDevice();
}

void Service::HostapdEventCallback(HostapdMonitor::Event event,
                                   const std::string& data) {
  switch (event) {
    case HostapdMonitor::kHostapdFailed:
      adaptor_->SetState(kStateFailed);
      break;
    case HostapdMonitor::kHostapdStarted:
      adaptor_->SetState(kStateStarted);
      break;
    case HostapdMonitor::kStationConnected:
      LOG(INFO) << "Station connected: " << data;
      break;
    case HostapdMonitor::kStationDisconnected:
      LOG(INFO) << "Station disconnected: " << data;
      break;
    default:
      LOG(ERROR) << "Unknown event: " << event;
      break;
  }
}

}  // namespace apmanager
