// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROS_DISKS_RENAME_MANAGER_H_
#define CROS_DISKS_RENAME_MANAGER_H_

#include <map>
#include <string>

#include <base/macros.h>
#include <chromeos/dbus/service_constants.h>
#include <gtest/gtest_prod.h>

#include "cros-disks/sandboxed_process.h"

namespace brillo {

class ProcessImpl;

}  // namespace brillo

namespace cros_disks {

class Platform;
class RenameManagerObserverInterface;

class RenameManager {
 public:
  explicit RenameManager(Platform* platform);
  ~RenameManager();

  // Starts a renaming process of a given device.
  RenameErrorType StartRenaming(const std::string& device_path,
                                const std::string& device_file,
                                const std::string& volume_name,
                                const std::string& filesystem_type);

  // Handles a terminated renaming process.
  void RenamingFinished(pid_t pid, int status);

  void set_observer(RenameManagerObserverInterface* observer) {
    observer_ = observer;
  }

 private:
  FRIEND_TEST(RenameManagerTest, CanRename);
  FRIEND_TEST(RenameManagerTest, ValidateParameters);
  FRIEND_TEST(RenameManagerVolumeNameTest, ValidateParameters);

  // Platform service
  Platform* platform_;

  // Returns RENAME_ERROR_NONE if file system type is supported, and new
  // |volume_name| contains only allowed characters and length is not greater
  // than file system's limit.
  RenameErrorType ValidateParameters(const std::string& volume_name,
                                     const std::string& filesystem_type) const;

  // Returns true if renaming |source_path| is supported.
  bool CanRename(const std::string& source_path) const;

  // A list of outstanding renaming processes indexed by device path.
  std::map<std::string, SandboxedProcess> rename_process_;

  // Given the pid of renaming process it finds the device path.
  std::map<pid_t, std::string> pid_to_device_path_;

  RenameManagerObserverInterface* observer_;

  DISALLOW_COPY_AND_ASSIGN(RenameManager);
};

}  // namespace cros_disks

#endif  // CROS_DISKS_RENAME_MANAGER_H_
