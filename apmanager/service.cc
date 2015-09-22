// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apmanager/service.h"

#include <signal.h>

#include <base/strings/stringprintf.h>
#include <chromeos/errors/error.h>

#if !defined(__ANDROID__)
#include <chromeos/dbus/service_constants.h>
#else
#include "dbus/apmanager/dbus-constants.h"
#endif  // __ANDROID__

#include "apmanager/manager.h"

using chromeos::dbus_utils::AsyncEventSequencer;
using chromeos::dbus_utils::ExportedObjectManager;
using org::chromium::apmanager::ManagerAdaptor;
using std::string;

namespace apmanager {

// static.
const char Service::kHostapdPath[] = "/usr/sbin/hostapd";
const char Service::kHostapdConfigPathFormat[] =
    "/var/run/apmanager/hostapd/hostapd-%d.conf";
const char Service::kHostapdControlInterfacePath[] =
    "/var/run/apmanager/hostapd/ctrl_iface";
const int Service::kTerminationTimeoutSeconds = 2;

// static. Service state definitions.
const char Service::kStateIdle[] = "Idle";
const char Service::kStateStarting[] = "Starting";
const char Service::kStateStarted[] = "Started";
const char Service::kStateFailed[] = "Failed";

Service::Service(Manager* manager, int service_identifier)
    : org::chromium::apmanager::ServiceAdaptor(this),
      manager_(manager),
      identifier_(service_identifier),
      service_path_(
          base::StringPrintf("%s/services/%d",
                             ManagerAdaptor::GetObjectPath().value().c_str(),
                             service_identifier)),
      dbus_path_(dbus::ObjectPath(service_path_)),
      config_(new Config(manager, service_path_)),
      dhcp_server_factory_(DHCPServerFactory::GetInstance()),
      file_writer_(FileWriter::GetInstance()),
      process_factory_(ProcessFactory::GetInstance()) {
  SetConfig(config_->dbus_path());
  SetState(kStateIdle);
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

void Service::RegisterAsync(ExportedObjectManager* object_manager,
                            const scoped_refptr<dbus::Bus>& bus,
                            AsyncEventSequencer* sequencer) {
  CHECK(!dbus_object_) << "Already registered";
  dbus_object_.reset(
      new chromeos::dbus_utils::DBusObject(
          object_manager,
          bus,
          dbus_path_));
  RegisterWithDBusObject(dbus_object_.get());
  dbus_object_->RegisterAsync(
      sequencer->GetHandler("Service.RegisterAsync() failed.", true));

  // Register Config DBus object.
  config_->RegisterAsync(object_manager, bus, sequencer);
}

bool Service::Start(chromeos::ErrorPtr* error) {
  if (IsHostapdRunning()) {
    chromeos::Error::AddTo(
        error, FROM_HERE, chromeos::errors::dbus::kDomain, kServiceError,
        "Service already running");
    return false;
  }

  // Setup hostapd control interface path.
  config_->set_control_interface(kHostapdControlInterfacePath);

  // Generate hostapd configuration content.
  string config_str;
  if (!config_->GenerateConfigFile(error, &config_str)) {
    chromeos::Error::AddTo(
        error, FROM_HERE, chromeos::errors::dbus::kDomain, kServiceError,
        "Failed to generate config file");
    return false;
  }

  // Write configuration to a file.
  string config_file_name = base::StringPrintf(kHostapdConfigPathFormat,
                                               identifier_);
  if (!file_writer_->Write(config_file_name, config_str)) {
    chromeos::Error::AddTo(
        error, FROM_HERE, chromeos::errors::dbus::kDomain, kServiceError,
        "Failed to write configuration to a file");
    return false;
  }

  // Claim the device needed for this ap service.
  if (!config_->ClaimDevice()) {
    chromeos::Error::AddTo(
        error, FROM_HERE, chromeos::errors::dbus::kDomain, kServiceError,
        "Failed to claim the device for this service");
    return false;
  }

  // Start hostapd process.
  if (!StartHostapdProcess(config_file_name)) {
    chromeos::Error::AddTo(
        error, FROM_HERE, chromeos::errors::dbus::kDomain, kServiceError,
        "Failed to start hostapd");
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
      chromeos::Error::AddTo(
          error, FROM_HERE, chromeos::errors::dbus::kDomain, kServiceError,
          "Failed to start DHCP server");
      ReleaseResources();
      return false;
    }
    manager_->RequestDHCPPortAccess(config_->selected_interface());
  }

  // Start monitoring hostapd.
  if (!hostapd_monitor_) {
    hostapd_monitor_.reset(
        new HostapdMonitor(base::Bind(&Service::HostapdEventCallback,
                                      base::Unretained(this)),
                           config_->control_interface(),
                           config_->selected_interface()));
  }
  hostapd_monitor_->Start();

  // Update service state.
  SetState(kStateStarting);

  return true;
}

bool Service::Stop(chromeos::ErrorPtr* error) {
  if (!IsHostapdRunning()) {
    chromeos::Error::AddTo(
        error, FROM_HERE, chromeos::errors::dbus::kDomain, kServiceError,
        "Service is not currently running");
    return false;
  }

  ReleaseResources();
  SetState(kStateIdle);
  return true;
}

bool Service::IsHostapdRunning() {
  return hostapd_process_ && hostapd_process_->pid() != 0 &&
         chromeos::Process::ProcessExists(hostapd_process_->pid());
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
  config_->ReleaseDevice();
  manager_->ReleaseDHCPPortAccess(config_->selected_interface());
}

void Service::HostapdEventCallback(HostapdMonitor::Event event,
                                   const std::string& data) {
  switch (event) {
    case HostapdMonitor::kHostapdFailed:
      SetState(kStateFailed);
      break;
    case HostapdMonitor::kHostapdStarted:
      SetState(kStateStarted);
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
