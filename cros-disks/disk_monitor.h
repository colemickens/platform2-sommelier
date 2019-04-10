// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROS_DISKS_DISK_MONITOR_H_
#define CROS_DISKS_DISK_MONITOR_H_

#include <libudev.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <base/callback.h>
#include <base/files/file_path.h>
#include <base/macros.h>
#include <gtest/gtest_prod.h>

#include "cros-disks/device_event.h"
#include "cros-disks/device_event_source_interface.h"
#include "cros-disks/disk.h"
#include "cros-disks/mount_manager.h"

namespace cros_disks {

// The DiskMonitor is responsible for reading device state from udev.
// Said changes could be the result of a udev notification or a synchronous
// call to enumerate the relevant storage devices attached to the system.
//
// This class is designed to run within a single-threaded GMainLoop application
// and should not be considered thread safe.
class DiskMonitor : public DeviceEventSourceInterface {
 public:
  DiskMonitor();
  ~DiskMonitor() override;

  // Initializes the disk monitor.
  // Returns true on success.
  virtual bool Initialize();

  // Lists the current block devices attached to the system.
  virtual std::vector<Disk> EnumerateDisks() const;

  // Gets a Disk object that corresponds to a given device file.
  virtual bool GetDiskByDevicePath(const base::FilePath& device_path,
                                   Disk* disk) const;

  // Checks if the device path points to a device handled by this class.
  bool IsPathRecognized(const base::FilePath& path) const;

  // A file descriptor that can be select()ed or poll()ed for system changes.
  int udev_monitor_fd() const { return udev_monitor_fd_; }

  // Implements the DeviceEventSourceInterface interface to read the changes
  // from udev and converts the changes into device events. Returns false on
  // error or if not device event is available. Must be called to clear the fd.
  bool GetDeviceEvents(DeviceEventList* events) override;

 private:
  // An EnumerateBlockDevices callback that emulates a block device event
  // defined by |action| on |device|. Always returns true to continue
  // enumeration in EnumerateBlockDevices.
  bool EmulateBlockDeviceEvent(const char* action, udev_device* device);

  // Enumerates the block devices on the system and invokes |callback| for each
  // device found during the enumeration. The ownership of |udev_device| is not
  // transferred to |callback|. The enumeration stops if |callback| returns
  // false.
  void EnumerateBlockDevices(
      const base::Callback<bool(udev_device* dev)>& callback) const;

  // Determines one or more device/disk events from a udev block device change.
  void ProcessBlockDeviceEvents(udev_device* device,
                                const char* action,
                                DeviceEventList* events);

  // Determines one or more device/disk events from a udev MMC or SCSI device
  // change.
  void ProcessMmcOrScsiDeviceEvents(udev_device* device,
                                    const char* action,
                                    DeviceEventList* events);

  // The root udev object.
  mutable udev* udev_;

  // Provides access to udev changes as they occur.
  udev_monitor* udev_monitor_;

  // A file descriptor that indicates changes to the system.
  int udev_monitor_fd_;

  // A set of device sysfs paths detected by the udev monitor.
  std::set<std::string> devices_detected_;

  // A mapping from a sysfs path of a disk, detected by the udev monitor,
  // to a set of sysfs paths of the immediate children of the disk.
  std::map<std::string, std::set<std::string>> disks_detected_;

  DISALLOW_COPY_AND_ASSIGN(DiskMonitor);
};

}  // namespace cros_disks

#endif  // CROS_DISKS_DISK_MONITOR_H_
