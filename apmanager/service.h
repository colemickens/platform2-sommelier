// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APMANAGER_SERVICE_H_
#define APMANAGER_SERVICE_H_

#include <string>

#include <base/macros.h>
#include <base/memory/weak_ptr.h>
#include <brillo/process.h>

#include "apmanager/config.h"
#include "apmanager/dhcp_server_factory.h"
#include "apmanager/file_writer.h"
#include "apmanager/hostapd_monitor.h"
#include "apmanager/process_factory.h"
#include "dbus_bindings/org.chromium.apmanager.Service.h"

namespace apmanager {

class Manager;

#if defined(__BRILLO__)
class EventDispatcher;
#endif  // __BRILLO__

class Service : public org::chromium::apmanager::ServiceAdaptor,
                public org::chromium::apmanager::ServiceInterface {
 public:
  Service(Manager* manager, int service_identifier);
  virtual ~Service();

  // Implementation of org::chromium::apmanager::ServiceInterface.
  void Start(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<>> response);
  bool Stop(brillo::ErrorPtr* error);

  // Register Service DBus object.
  void RegisterAsync(
      brillo::dbus_utils::ExportedObjectManager* object_manager,
      const scoped_refptr<dbus::Bus>& bus,
      brillo::dbus_utils::AsyncEventSequencer* sequencer);

  const dbus::ObjectPath& dbus_path() const { return dbus_path_; }

  int identifier() const { return identifier_; }

 private:
  friend class ServiceTest;

  static const char kHostapdPath[];
  static const char kHostapdConfigPathFormat[];
  static const char kHostapdControlInterfacePath[];
  static const int kTerminationTimeoutSeconds;
  static const char kStateIdle[];
  static const char kStateStarting[];
  static const char kStateStarted[];
  static const char kStateFailed[];

#if defined(__BRILLO__)
  static const int kAPInterfaceCheckIntervalMilliseconds;
  static const int kAPInterfaceCheckMaxAttempts;

  // Task to check enumeration status of the specified AP interface
  // |interface_name|.
  void APInterfaceCheckTask(
      const std::string& interface_name,
      int check_count,
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<>> response);

  // Handle asynchronous service start failures.
  void HandleStartFailure();
#endif  // __BRILLO__

  bool StartInternal(brillo::ErrorPtr* error);

  // Return true if hostapd process is currently running.
  bool IsHostapdRunning();

  // Start hostapd process. Return true if process is created/started
  // successfully, false otherwise.
  bool StartHostapdProcess(const std::string& config_file_path);

  // Stop the running hostapd process. Sending it a SIGTERM signal first, then
  // a SIGKILL if failed to terminated with SIGTERM.
  void StopHostapdProcess();

  // Release resources allocated to this service.
  void ReleaseResources();

  void HostapdEventCallback(HostapdMonitor::Event event,
                            const std::string& data);

  Manager* manager_;
  int identifier_;
  std::string service_path_;
  dbus::ObjectPath dbus_path_;
  std::unique_ptr<Config> config_;
  std::unique_ptr<brillo::dbus_utils::DBusObject> dbus_object_;
  std::unique_ptr<brillo::Process> hostapd_process_;
  std::unique_ptr<DHCPServer> dhcp_server_;
  DHCPServerFactory* dhcp_server_factory_;
  FileWriter* file_writer_;
  ProcessFactory* process_factory_;
  std::unique_ptr<HostapdMonitor> hostapd_monitor_;
#if defined(__BRILLO__)
  EventDispatcher* event_dispatcher_;
  bool start_in_progress_;
#endif  // __BRILLO__

  base::WeakPtrFactory<Service> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(Service);
};

}  // namespace apmanager

#endif  // APMANAGER_SERVICE_H_
