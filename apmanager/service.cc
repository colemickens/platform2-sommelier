// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apmanager/service.h"

#include <signal.h>

#include <base/strings/stringprintf.h>
#include <brillo/errors/error.h>

#if !defined(__ANDROID__)
#include <chromeos/dbus/service_constants.h>
#else
#include "dbus/apmanager/dbus-constants.h"
#endif  // __ANDROID__

#if defined(__BRILLO__)
#include <base/bind.h>

#include "apmanager/event_dispatcher.h"
#endif  // __BRILLO__

#include "apmanager/manager.h"

using brillo::dbus_utils::AsyncEventSequencer;
using brillo::dbus_utils::DBusMethodResponse;
using brillo::dbus_utils::ExportedObjectManager;
using org::chromium::apmanager::ManagerAdaptor;
using std::string;

namespace apmanager {

// static.
#if !defined(__ANDROID__)
const char Service::kHostapdPath[] = "/usr/sbin/hostapd";
const char Service::kHostapdConfigPathFormat[] =
    "/var/run/apmanager/hostapd/hostapd-%d.conf";
const char Service::kHostapdControlInterfacePath[] =
    "/var/run/apmanager/hostapd/ctrl_iface";
#else
const char Service::kHostapdPath[] = "/system/bin/hostapd";
const char Service::kHostapdConfigPathFormat[] =
    "/data/misc/apmanager/hostapd/hostapd-%d.conf";
const char Service::kHostapdControlInterfacePath[] =
    "/data/misc/apmanager/hostapd/ctrl_iface";
#endif  // __ANDROID__

#if defined(__BRILLO__)
const int Service::kAPInterfaceCheckIntervalMilliseconds = 200;
const int Service::kAPInterfaceCheckMaxAttempts = 5;
#endif  // __BRILLO__

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

#if defined(__BRILLO__)
  event_dispatcher_ = EventDispatcher::GetInstance();
  start_in_progress_ = false;
#endif
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
      new brillo::dbus_utils::DBusObject(
          object_manager,
          bus,
          dbus_path_));
  RegisterWithDBusObject(dbus_object_.get());
  dbus_object_->RegisterAsync(
      sequencer->GetHandler("Service.RegisterAsync() failed.", true));

  // Register Config DBus object.
  config_->RegisterAsync(object_manager, bus, sequencer);
}

bool Service::StartInternal(brillo::ErrorPtr* error) {
  if (IsHostapdRunning()) {
    brillo::Error::AddTo(
        error, FROM_HERE, brillo::errors::dbus::kDomain, kServiceError,
        "Service already running");
    return false;
  }

  // Setup hostapd control interface path.
  config_->set_control_interface(kHostapdControlInterfacePath);

  // Generate hostapd configuration content.
  string config_str;
  if (!config_->GenerateConfigFile(error, &config_str)) {
    brillo::Error::AddTo(
        error, FROM_HERE, brillo::errors::dbus::kDomain, kServiceError,
        "Failed to generate config file");
    return false;
  }

  // Write configuration to a file.
  string config_file_name = base::StringPrintf(kHostapdConfigPathFormat,
                                               identifier_);
  if (!file_writer_->Write(config_file_name, config_str)) {
    brillo::Error::AddTo(
        error, FROM_HERE, brillo::errors::dbus::kDomain, kServiceError,
        "Failed to write configuration to a file");
    return false;
  }

  // Claim the device needed for this ap service.
  if (!config_->ClaimDevice()) {
    brillo::Error::AddTo(
        error, FROM_HERE, brillo::errors::dbus::kDomain, kServiceError,
        "Failed to claim the device for this service");
    return false;
  }

  // Start hostapd process.
  if (!StartHostapdProcess(config_file_name)) {
    brillo::Error::AddTo(
        error, FROM_HERE, brillo::errors::dbus::kDomain, kServiceError,
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
      brillo::Error::AddTo(
          error, FROM_HERE, brillo::errors::dbus::kDomain, kServiceError,
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
                                      weak_factory_.GetWeakPtr()),
                           config_->control_interface(),
                           config_->selected_interface()));
  }
  hostapd_monitor_->Start();

  // Update service state.
  SetState(kStateStarting);

  return true;
}

void Service::Start(std::unique_ptr<DBusMethodResponse<>> response) {
  brillo::ErrorPtr error;

#if !defined(__BRILLO__)
  if (!StartInternal(&error)) {
    response->ReplyWithError(error.get());
  } else {
    response->Return();
  }
#else
  if (start_in_progress_) {
    brillo::Error::AddTo(
        &error, FROM_HERE, brillo::errors::dbus::kDomain, kServiceError,
        "Start already in progress");
    response->ReplyWithError(error.get());
    return;
  }

  string interface_name;
  if (!manager_->SetupApModeInterface(&interface_name)) {
    brillo::Error::AddTo(
        &error, FROM_HERE, brillo::errors::dbus::kDomain, kServiceError,
        "Failed to setup AP mode interface");
    response->ReplyWithError(error.get());
    return;
  }

  event_dispatcher_->PostDelayedTask(
      base::Bind(&Service::APInterfaceCheckTask,
                 weak_factory_.GetWeakPtr(),
                 interface_name,
                 0,    // Initial check count.
                 base::Passed(&response)),
      kAPInterfaceCheckIntervalMilliseconds);
#endif
}

bool Service::Stop(brillo::ErrorPtr* error) {
  if (!IsHostapdRunning()) {
    brillo::Error::AddTo(
        error, FROM_HERE, brillo::errors::dbus::kDomain, kServiceError,
        "Service is not currently running");
    return false;
  }

  ReleaseResources();
  SetState(kStateIdle);
  return true;
}

#if defined(__BRILLO__)
void Service::HandleStartFailure() {
  // Restore station mode interface.
  string station_mode_interface;
  manager_->SetupStationModeInterface(&station_mode_interface);

  // Reset state variables.
  start_in_progress_ = false;
}

void Service::APInterfaceCheckTask(
    const string& interface_name,
    int check_count,
    std::unique_ptr<DBusMethodResponse<>> response) {
  brillo::ErrorPtr error;

  // Check if the AP interface is enumerated.
  if (manager_->GetDeviceFromInterfaceName(interface_name)) {
    // Explicitly set the interface name to avoid picking other interface.
    config_->SetInterfaceName(interface_name);
    if (!StartInternal(&error)) {
      HandleStartFailure();
      response->ReplyWithError(error.get());
    } else {
      response->Return();
    }
    return;
  }

  check_count++;
  if (check_count >= kAPInterfaceCheckMaxAttempts) {
    brillo::Error::AddTo(
        &error, FROM_HERE, brillo::errors::dbus::kDomain, kServiceError,
        "Timeout waiting for AP interface to be enumerated");
    HandleStartFailure();
    response->ReplyWithError(error.get());
    return;
  }

  event_dispatcher_->PostDelayedTask(
      base::Bind(&Service::APInterfaceCheckTask,
                 weak_factory_.GetWeakPtr(),
                 interface_name,
                 check_count,
                 base::Passed(&response)),
      kAPInterfaceCheckIntervalMilliseconds);
}
#endif  // __BRILLO__

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
  config_->ReleaseDevice();
  manager_->ReleaseDHCPPortAccess(config_->selected_interface());
#if defined(__BRILLO__)
  // Restore station mode interface.
  string station_mode_interface;
  manager_->SetupStationModeInterface(&station_mode_interface);
#endif  // __BRILLO__
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
