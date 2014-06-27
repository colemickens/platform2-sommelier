// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROS_DISKS_DEVICE_EJECTOR_H_
#define CROS_DISKS_DEVICE_EJECTOR_H_

#include <list>
#include <string>

#include <base/basictypes.h>

namespace cros_disks {

class GlibProcess;

// A class for ejecting any removable media on a device.
class DeviceEjector {
 public:
  DeviceEjector();
  virtual ~DeviceEjector();

  // Ejects any removable media on a device at |device_path| using the
  // 'eject' program. Returns true if the eject process has launched
  // successfully (but may not complete until OnEjectProcessTerminated
  // is called).
  virtual bool Eject(const std::string& device_path);

 private:
  // Invoked when an eject process has terminated.
  void OnEjectProcessTerminated(GlibProcess* process);

  // List of outstanding eject processes.
  std::list<GlibProcess*> eject_processes_;

  DISALLOW_COPY_AND_ASSIGN(DeviceEjector);
};

}  // namespace cros_disks

#endif  // CROS_DISKS_DEVICE_EJECTOR_H_
