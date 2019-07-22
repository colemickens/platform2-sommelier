// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBBRILLO_BRILLO_UDEV_MOCK_UDEV_H_
#define LIBBRILLO_BRILLO_UDEV_MOCK_UDEV_H_

#include <memory>

#include <brillo/brillo_export.h>
#include <brillo/udev/udev.h>
#include <brillo/udev/udev_device.h>
#include <brillo/udev/udev_enumerate.h>
#include <brillo/udev/udev_monitor.h>
#include <gmock/gmock.h>

namespace brillo {

class BRILLO_EXPORT MockUdev : public Udev {
 public:
  MockUdev() : Udev(nullptr) {}
  ~MockUdev() override = default;

  MOCK_METHOD1(CreateDeviceFromSysPath,
               std::unique_ptr<UdevDevice>(const char* sys_path));
  MOCK_METHOD2(CreateDeviceFromDeviceNumber,
               std::unique_ptr<UdevDevice>(char type, dev_t device_number));
  MOCK_METHOD2(CreateDeviceFromSubsystemSysName,
               std::unique_ptr<UdevDevice>(const char* subsystem,
                                           const char* sys_name));
  MOCK_METHOD0(CreateEnumerate, std::unique_ptr<UdevEnumerate>());
  MOCK_METHOD1(CreateMonitorFromNetlink,
               std::unique_ptr<UdevMonitor>(const char* name));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockUdev);
};

}  // namespace brillo

#endif  // LIBBRILLO_BRILLO_UDEV_MOCK_UDEV_H_
