// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BRDEBUG_DEVICE_PROPERTY_WATCHER_H_
#define BRDEBUG_DEVICE_PROPERTY_WATCHER_H_

#include <map>
#include <string>

#include <base/callback.h>
#include <base/files/file_path.h>
#include <base/files/file_path_watcher.h>
#include <base/memory/weak_ptr.h>

namespace brdebug {

// Reads and watches files that store Brillo device properties.
class DevicePropertyWatcher {
 public:
  explicit DevicePropertyWatcher(const base::Closure& callback);
  virtual ~DevicePropertyWatcher() = default;

  // Initializes the device property watcher. Returns true on success.
  bool Init();
  // Getter for the device properties.
  std::map<std::string, std::string> GetDeviceProperties() const;

 private:
  // Callback function for the FilePathWatcher object.
  void HandleFileChange(const base::FilePath& path, bool error);
  // Reads device properties from the file system.
  void ReadDeviceProperties();

  base::FilePath device_property_dir_path_;
  base::FilePathWatcher device_property_dir_watcher_;
  std::map<std::string, std::string> device_properties_;
  base::Closure device_property_change_callback_;

  base::WeakPtrFactory<DevicePropertyWatcher> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(DevicePropertyWatcher);
};

}  // namespace brdebug

#endif  // BRDEBUG_DEVICE_PROPERTY_WATCHER_H_
