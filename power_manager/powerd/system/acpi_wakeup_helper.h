// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SYSTEM_ACPI_WAKEUP_HELPER_H_
#define POWER_MANAGER_POWERD_SYSTEM_ACPI_WAKEUP_HELPER_H_

#include <string>

#include <base/basictypes.h>
#include <base/memory/scoped_ptr.h>

namespace power_manager {
namespace system {

// Abstraction layer around /proc/acpi/wakeup so that we can substitute it for
// testing. We cannot just use a regular file because read/write have special
// semantics.
class AcpiWakeupFileInterface {
 public:
  AcpiWakeupFileInterface() {}
  virtual ~AcpiWakeupFileInterface() {}

  // Checks whether the file exists.
  virtual bool Exists() = 0;

  // Reads file contents. Returns true on success.
  virtual bool Read(std::string* contents) = 0;

  // Writes file contents. Returns true on success.
  virtual bool Write(const std::string& contents) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(AcpiWakeupFileInterface);
};

class AcpiWakeupHelper {
 public:
  AcpiWakeupHelper();

  // Forces use of a fake implementation instead of /proc/acpi/wakeup. Only for
  // testing.
  void set_file_for_testing(scoped_ptr<AcpiWakeupFileInterface> file);

  // Checks whether /proc/acpi/wakeup is available on this system.
  bool IsSupported();

  // Determines whether ACPI wakeup is enabled for a given device. Returns true
  // on success.
  bool GetWakeupEnabled(const std::string& device_name, bool* enabled_out);

  // Enables or disables ACPI wakeup for a given device. Returns true on
  // success.
  bool SetWakeupEnabled(const std::string& device_name, bool enabled);

 private:
  // Toggles ACPI wakeup for a given device. Used internally by
  // SetWakeupEnabled, since the kernel interface does not expose an interface
  // to set it directly.
  bool ToggleWakeupEnabled(const std::string& device_name);

  scoped_ptr<AcpiWakeupFileInterface> file_;

  DISALLOW_COPY_AND_ASSIGN(AcpiWakeupHelper);
};

}  // namespace system
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SYSTEM_ACPI_WAKEUP_HELPER_H_
