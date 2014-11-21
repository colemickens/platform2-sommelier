// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apmanager/service.h"

#include <signal.h>

#include <base/files/file_util.h>
#include <base/strings/stringprintf.h>
#include <chromeos/dbus/service_constants.h>
#include <chromeos/errors/error.h>

#include "apmanager/manager.h"

using chromeos::dbus_utils::AsyncEventSequencer;
using chromeos::dbus_utils::ExportedObjectManager;
using std::string;

namespace apmanager {

// static.
const char Service::kHostapdPath[] = "/usr/sbin/hostapd";
const char Service::kHostapdConfigPathFormat[] = "/tmp/hostapd-%d";
const int Service::kTerminationTimeoutSeconds = 2;

Service::Service(Manager* manager, int service_identifier)
    : org::chromium::apmanager::ServiceAdaptor(this),
      manager_(manager),
      service_identifier_(service_identifier),
      service_path_(base::StringPrintf("%s/services/%d",
                                       kManagerPath,
                                       service_identifier)),
      dbus_path_(dbus::ObjectPath(service_path_)),
      config_(new Config(manager, service_path_)) {
  SetConfig(config_->dbus_path());
}

Service::~Service() {
  // Stop hostapd process if still running.
  if (IsHostapdRunning()) {
    StopHostapdProcess();
    // Release device used by this hostapd instance.
    config_->ReleaseDevice();
  }
}

void Service::RegisterAsync(ExportedObjectManager* object_manager,
                            AsyncEventSequencer* sequencer) {
  CHECK(!dbus_object_) << "Already registered";
  dbus_object_.reset(
      new chromeos::dbus_utils::DBusObject(
          object_manager,
          object_manager ? object_manager->GetBus() : nullptr,
          dbus_path_));
  RegisterWithDBusObject(dbus_object_.get());
  dbus_object_->RegisterAsync(
      sequencer->GetHandler("Service.RegisterAsync() failed.", true));

  // Register Config DBus object.
  config_->RegisterAsync(object_manager, sequencer);
}

bool Service::Start(chromeos::ErrorPtr* error) {
  if (IsHostapdRunning()) {
    chromeos::Error::AddTo(
        error, FROM_HERE, chromeos::errors::dbus::kDomain, kServiceError,
        "Service already running");
    return false;
  }

  // Generate hostapd configuration content.
  string config_str;
  if (!config_->GenerateConfigFile(error, &config_str)) {
    chromeos::Error::AddTo(
        error, FROM_HERE, chromeos::errors::dbus::kDomain, kServiceError,
        "Failed to generate config file");
    return false;
  }

  // Write configuration to a file.
  base::FilePath file_path(base::StringPrintf(kHostapdConfigPathFormat,
                                              service_identifier_));
  if (base::WriteFile(file_path, config_str.c_str(), config_str.size()) == -1) {
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
  if (!StartHostapdProcess(file_path.value())) {
    chromeos::Error::AddTo(
        error, FROM_HERE, chromeos::errors::dbus::kDomain, kServiceError,
        "Failed to start hostapd");
    // Release the device claimed for this service.
    config_->ReleaseDevice();
    return false;
  }

  return true;
}

bool Service::Stop(chromeos::ErrorPtr* error) {
  if (!IsHostapdRunning()) {
    chromeos::Error::AddTo(
        error, FROM_HERE, chromeos::errors::dbus::kDomain, kServiceError,
        "Service is not currently running");
    return false;
  }

  StopHostapdProcess();
  // Release the device used by this hostapd instance.
  config_->ReleaseDevice();
  return true;
}

bool Service::IsHostapdRunning() {
  return hostapd_process_ && hostapd_process_->pid() != 0 &&
         chromeos::Process::ProcessExists(hostapd_process_->pid());
}

bool Service::StartHostapdProcess(const string& config_file_path) {
  hostapd_process_.reset(new chromeos::ProcessImpl());
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

}  // namespace apmanager
