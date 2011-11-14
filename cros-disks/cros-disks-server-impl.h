// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROS_DISKS_SERVER_IMPL_H_
#define CROS_DISKS_SERVER_IMPL_H_

#include <string>
#include <vector>

#include "cros-disks/cros-disks-server.h"
#include "cros-disks/device-event-dispatcher-interface.h"
#include "cros-disks/device-event-queue.h"
#include "cros-disks/disk.h"
#include "cros-disks/power-manager-observer-interface.h"
#include "cros-disks/session-manager-observer-interface.h"

namespace cros_disks {

class ArchiveManager;
class DeviceEvent;
class DiskManager;
class FormatManager;
class MountManager;
class Platform;

// The d-bus server for the cros-disks daemon.
//
// Example Usage:
//
// DBus::Connection server_conn = DBus::Connection::SystemBus();
// server_conn.request_name("org.chromium.CrosDisks");
// ArchiveManager archive_manager(...);
// DiskManager disk_manager(...);
// FormatManager format_manager;
// CrosDisksServer* server = new(std::nothrow)
//     CrosDisksServer(server_conn, &platform,
//                     &archive_manager,
//                     &disk_manager, &format_manager);
//
// At this point the server should be attached to the main loop.
//
class CrosDisksServer : public org::chromium::CrosDisks_adaptor,
                        public DBus::IntrospectableAdaptor,
                        public DBus::PropertiesAdaptor,
                        public DBus::ObjectAdaptor,
                        public DeviceEventDispatcherInterface,
                        public SessionManagerObserverInterface {
 public:
  CrosDisksServer(DBus::Connection& connection,  // NOLINT
                  Platform* platform,
                  ArchiveManager* archive_manager,
                  DiskManager* disk_manager,
                  FormatManager* format_manager);
  virtual ~CrosDisksServer();

  // Called by FormatManager when the formatting is finished
  virtual void SignalFormattingFinished(const std::string& device_path,
      int status);

  // A method for asynchronous formating device using specified file system.
  // Assumes device path is vaild (should it be invaild singal
  // FormattingFinished(false) will be sent)
  // Return true if formatting is successfully INITIALIZED, rather than finished
  virtual bool FormatDevice(const std::string& device_path,
      const std::string& filesystem, ::DBus::Error &error);  // NOLINT

  // A method for checking if the daemon is running. Always returns true.
  virtual bool IsAlive(DBus::Error& error);  // NOLINT

  // Mounts a path when invoked.
  virtual void Mount(const std::string& path,
      const std::string& filesystem_type,
      const std::vector<std::string>& options,
      DBus::Error& error);  // NOLINT

  // Unmounts a path when invoked.
  virtual void Unmount(const std::string& path,
      const std::vector<std::string>& options,
      DBus::Error& error);  // NOLINT

  // Unmounts all paths mounted by Mount() when invoked.
  virtual void UnmountAll(DBus::Error& error);  // NOLINT

  // Returns a list of device sysfs paths for all disk devices attached to
  // the system.
  virtual std::vector<std::string> EnumerateDevices(
      DBus::Error& error);  // NOLINT

  // Returns a list of device sysfs paths for all auto-mountable disk devices
  // attached to the system. Currently, all external disk devices, which are
  // neither on the boot device nor virtual, are considered auto-mountable.
  virtual std::vector<std::string> EnumerateAutoMountableDevices(
      DBus::Error& error);  // NOLINT

  // Returns properties of a disk device attached to the system.
  virtual DBusDisk GetDeviceProperties(const std::string& device_path,
      DBus::Error& error);  // NOLINT

  // Implements the SessionManagerObserverInterface interface to handle
  // the event when the session has been started.
  virtual void OnSessionStarted(const std::string& user);

  // Implements the SessionManagerObserverInterface interface to handle
  // the event when the session has been stopped.
  virtual void OnSessionStopped(const std::string& user);

 private:
  // Implements the DeviceEventDispatcherInterface to dispatch a device event
  // by emitting the corresponding D-Bus signal.
  void DispatchDeviceEvent(const DeviceEvent& event);

  // Initializes DBus properties.
  void InitializeProperties();

  // Overrides PropertiesAdaptor::on_set_property to handle
  // org.freedesktop.DBus.Properties.Set calls.
  virtual void on_set_property(DBus::InterfaceAdaptor& interface,  // NOLINT
      const std::string& property, const DBus::Variant& value);

  // Returns a list of device sysfs paths for all disk devices attached to
  // the system. If auto_mountable_only is true, only auto-mountable disk
  // devices are returned.
  std::vector<std::string> DoEnumerateDevices(bool auto_mountable_only) const;

  // Unmounts all paths mounted by Mount().
  void DoUnmountAll();

  Platform* platform_;

  ArchiveManager* archive_manager_;

  DiskManager* disk_manager_;

  FormatManager* format_manager_;

  std::vector<MountManager*> mount_managers_;
};

}  // namespace cros_disks

#endif  // CROS_DISKS_SERVER_IMPL_H_
