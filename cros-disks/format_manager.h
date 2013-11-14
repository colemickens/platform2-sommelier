// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROS_DISKS_FORMAT_MANAGER_H_
#define CROS_DISKS_FORMAT_MANAGER_H_

#include <map>
#include <string>

#include <base/basictypes.h>
#include <chromeos/dbus/service_constants.h>
#include <gtest/gtest_prod.h>

namespace chromeos {

class ProcessImpl;

}  // namespace chromeos

namespace cros_disks {

class FormatManagerObserverInterface;

class FormatManager {
 public:
  FormatManager();
  ~FormatManager();

  // Starts a formatting process of a given device.
  FormatErrorType StartFormatting(const std::string& device_path,
                                  const std::string& filesystem);

  // Stops a formatting process of a given device.
  FormatErrorType StopFormatting(const std::string& device_path);

  // Handles a terminated formatting process.
  void FormattingFinished(pid_t pid, int status);

  void set_observer(FormatManagerObserverInterface* observer) {
    observer_ = observer;
  }

 private:
  // Returns the full path of an external formatting program if it is
  // found in some predefined locations. Otherwise, an empty string is
  // returned.
  std::string GetFormatProgramPath(const std::string& filesystem) const;

  // Returns true if formatting a given file system is supported.
  bool IsFilesystemSupported(const std::string& filesystem) const;

  // A list of outstanding formatting processes indexed by device path.
  std::map<std::string, chromeos::ProcessImpl> format_process_;

  // Given the pid of formatting process it finds the device path.
  std::map<pid_t, std::string> pid_to_device_path_;

  FormatManagerObserverInterface* observer_;

  FRIEND_TEST(FormatManagerTest, GetFormatProgramPath);
  FRIEND_TEST(FormatManagerTest, IsFilesystemSupported);

  DISALLOW_COPY_AND_ASSIGN(FormatManager);
};

}  // namespace cros_disks

#endif  // CROS_DISKS_FORMAT_MANAGER_H_
