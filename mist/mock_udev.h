// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MIST_MOCK_UDEV_H_
#define MIST_MOCK_UDEV_H_

#include <memory>

#include <gmock/gmock.h>

#include "mist/udev.h"
#include "mist/udev_device.h"
#include "mist/udev_enumerate.h"
#include "mist/udev_monitor.h"

namespace mist {

class MockUdev : public Udev {
 public:
  MockUdev() = default;
  ~MockUdev() override = default;

  MOCK_METHOD0(Initialize, bool());
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

}  // namespace mist

#endif  // MIST_MOCK_UDEV_H_
