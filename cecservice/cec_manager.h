// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CECSERVICE_CEC_MANAGER_H_
#define CECSERVICE_CEC_MANAGER_H_

#include <map>
#include <memory>
#include <string>

#include <base/memory/weak_ptr.h>
#include <base/files/file_path.h>

#include "cecservice/cec_device.h"
#include "cecservice/udev.h"

namespace cecservice {

// Main service object that maintains list of /dev/cec* nodes (with a help of
// udev) and passes received commands to CEC devices.
class CecManager {
 public:
  CecManager(const UdevFactory& udev_factory,
             const CecDeviceFactory& cec_factory);
  ~CecManager();

  // Sends wake up (image view on + active source ) request to all cec devices.
  void SetWakeUp();

  // Passes stand by command to all CEC devices.
  void SetStandBy();

 private:
  // Called when udev reports that new device has been added.
  void OnDeviceAdded(const base::FilePath& device_path);

  // Called when udev reports that new device has been removed.
  void OnDeviceRemoved(const base::FilePath& device_path);

  // Enumerates and adds all existing devices.
  void EnumerateAndAddExistingDevices();

  // Creates new handler for a device with a given path.
  void AddNewDevice(const base::FilePath& path);

  // Factory of CEC device handlers.
  const CecDeviceFactory& cec_factory_;

  // List of currently opened CEC devices.
  std::map<base::FilePath, std::unique_ptr<CecDevice>> devices_;

  // Udev object used to communicate with libudev.
  std::unique_ptr<Udev> udev_;

  base::WeakPtrFactory<CecManager> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CecManager);
};

}  // namespace cecservice

#endif  // CECSERVICE_CEC_MANAGER_H_
