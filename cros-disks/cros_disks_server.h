// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROS_DISKS_CROS_DISKS_SERVER_H_
#define CROS_DISKS_CROS_DISKS_SERVER_H_

#include <string>
#include <vector>

#include "cros-disks/dbus_adaptors/org.chromium.CrosDisks.h"
#include "cros-disks/device_event_dispatcher_interface.h"
#include "cros-disks/device_event_queue.h"
#include "cros-disks/disk.h"
#include "cros-disks/format_manager_observer_interface.h"
#include "cros-disks/mount_entry.h"
#include "cros-disks/session_manager_observer_interface.h"

namespace cros_disks {

class DiskManager;
class FormatManager;
class MountManager;
class Platform;

struct DeviceEvent;

// The d-bus server for the cros-disks daemon.
//
// Example Usage:
//
// DBus::Connection server_conn = DBus::Connection::SystemBus();
// CHECK(server_conn.acquire_name("org.chromium.CrosDisks"));
// ArchiveManager archive_manager(...);
// DiskManager disk_manager(...);
// FormatManager format_manager;
// CrosDisksServer* server = new(std::nothrow)
//     CrosDisksServer(server_conn, &platform,
//                     &disk_manager, &format_manager);
// server.RegisterMountManager(&disk_manager);
// server.RegisterMountManager(&archive_manager);
//
// At this point the server should be attached to the main loop.
//
class CrosDisksServer : public org::chromium::CrosDisks_adaptor,
                        public DBus::IntrospectableAdaptor,
                        public DBus::PropertiesAdaptor,
                        public DBus::ObjectAdaptor,
                        public DeviceEventDispatcherInterface,
                        public FormatManagerObserverInterface,
                        public SessionManagerObserverInterface {
 public:
  CrosDisksServer(DBus::Connection& connection,  // NOLINT
                  Platform* platform,
                  DiskManager* disk_manager,
                  FormatManager* format_manager);
  ~CrosDisksServer() override;

  // Registers a mount manager.
  void RegisterMountManager(MountManager* mount_manager);

  // A method for formatting a device specified by |path|.
  // On completion, a FormatCompleted signal is emitted to indicate whether
  // the operation succeeded or failed using a FormatErrorType enum value.
  void Format(const std::string& path,
              const std::string& filesystem_type,
              const std::vector<std::string>& options,
              DBus::Error& error) override;  // NOLINT

  // A method for checking if the daemon is running. Always returns true.
  bool IsAlive(DBus::Error& error) override;  // NOLINT

  // Mounts a path when invoked.
  void Mount(const std::string& path,
             const std::string& filesystem_type,
             const std::vector<std::string>& options,
             DBus::Error& error) override;  // NOLINT

  // Unmounts a path when invoked.
  void Unmount(const std::string& path,
               const std::vector<std::string>& options,
               DBus::Error& error) override;  // NOLINT

  // Unmounts all paths mounted by Mount() when invoked.
  void UnmountAll(DBus::Error& error) override;  // NOLINT

  // Returns a list of device sysfs paths for all disk devices attached to
  // the system.
  std::vector<std::string> EnumerateDevices(
      DBus::Error& error) override;  // NOLINT

  // Returns a list of device sysfs paths for all auto-mountable disk devices
  // attached to the system. Currently, all external disk devices, which are
  // neither on the boot device nor virtual, are considered auto-mountable.
  std::vector<std::string> EnumerateAutoMountableDevices(
      DBus::Error& error) override;  // NOLINT

  // Returns a list of mount entries (<error type, source path, source type,
  // mount path>) that are currently managed by cros-disks.
  DBusMountEntries EnumerateMountEntries(DBus::Error& error) override;  // NOLINT

  // Returns properties of a disk device attached to the system.
  DBusDisk GetDeviceProperties(const std::string& device_path,
                               DBus::Error& error) override;  // NOLINT

  // Implements the FormatManagerObserverInterface interface to handle
  // the event when a formatting operation has completed.
  void OnFormatCompleted(const std::string& device_path,
                         FormatErrorType error_type) override;

  // Implements the SessionManagerObserverInterface interface to handle
  // the event when the screen is locked.
  void OnScreenIsLocked() override;

  // Implements the SessionManagerObserverInterface interface to handle
  // the event when the screen is unlocked.
  void OnScreenIsUnlocked() override;

  // Implements the SessionManagerObserverInterface interface to handle
  // the event when the session has been started.
  void OnSessionStarted() override;

  // Implements the SessionManagerObserverInterface interface to handle
  // the event when the session has been stopped.
  void OnSessionStopped() override;

 private:
  // Implements the DeviceEventDispatcherInterface to dispatch a device event
  // by emitting the corresponding D-Bus signal.
  void DispatchDeviceEvent(const DeviceEvent& event) override;

  // Initializes DBus properties.
  void InitializeProperties();

  // Overrides PropertiesAdaptor::on_set_property to handle
  // org.freedesktop.DBus.Properties.Set calls.
  void on_set_property(DBus::InterfaceAdaptor& interface,  // NOLINT
      const std::string& property, const DBus::Variant& value) override;

  // Returns a list of device sysfs paths for all disk devices attached to
  // the system. If auto_mountable_only is true, only auto-mountable disk
  // devices are returned.
  std::vector<std::string> DoEnumerateDevices(bool auto_mountable_only) const;

  // Unmounts all paths mounted by Mount().
  void DoUnmountAll();

  Platform* platform_;

  DiskManager* disk_manager_;

  FormatManager* format_manager_;

  std::vector<MountManager*> mount_managers_;
};

}  // namespace cros_disks

#endif  // CROS_DISKS_CROS_DISKS_SERVER_H_
