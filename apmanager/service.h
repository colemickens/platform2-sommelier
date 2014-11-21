// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APMANAGER_SERVICE_H_
#define APMANAGER_SERVICE_H_

#include <string>

#include <base/macros.h>
#include <chromeos/process.h>

#include "apmanager/config.h"
#include "apmanager/dbus_adaptors/org.chromium.apmanager.Service.h"

namespace apmanager {

class Manager;

class Service : public org::chromium::apmanager::ServiceAdaptor,
                public org::chromium::apmanager::ServiceInterface {
 public:
  Service(Manager* manager, int service_identifier);
  virtual ~Service();

  // Implementation of ServiceInterface.
  virtual bool Start(chromeos::ErrorPtr* error);
  virtual bool Stop(chromeos::ErrorPtr* error);

  // Register Service DBus object.
  void RegisterAsync(
      chromeos::dbus_utils::ExportedObjectManager* object_manager,
      chromeos::dbus_utils::AsyncEventSequencer* sequencer);

  const dbus::ObjectPath& dbus_path() const { return dbus_path_; }

 private:
  friend class ServiceTest;

  static const char kHostapdPath[];
  static const char kHostapdConfigPathFormat[];
  static const char kHostapdControlInterfacePathFormat[];
  static const int kTerminationTimeoutSeconds;

  // Return true if hostapd process is currently running.
  bool IsHostapdRunning();

  // Start hostapd process. Return true if process is created/started
  // successfully, false otherwise.
  bool StartHostapdProcess(const std::string& config_file_path);

  // Stop the running hostapd process. Sending it a SIGTERM signal first, then
  // a SIGKILL if failed to terminated with SIGTERM.
  void StopHostapdProcess();

  Manager* manager_;
  int service_identifier_;
  std::string service_path_;
  dbus::ObjectPath dbus_path_;
  std::unique_ptr<Config> config_;
  std::unique_ptr<chromeos::dbus_utils::DBusObject> dbus_object_;
  std::unique_ptr<chromeos::Process> hostapd_process_;

  DISALLOW_COPY_AND_ASSIGN(Service);
};

}  // namespace apmanager

#endif  // APMANAGER_SERVICE_H_
