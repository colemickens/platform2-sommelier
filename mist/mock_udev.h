// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MIST_MOCK_UDEV_H_
#define MIST_MOCK_UDEV_H_

#include <base/compiler_specific.h>
#include <gmock/gmock.h>

#include "mist/udev.h"

namespace mist {

class MockUdev : public Udev {
 public:
  MockUdev();
  virtual ~MockUdev() OVERRIDE;

  MOCK_METHOD0(Initialize, bool());
  MOCK_METHOD1(CreateDeviceFromSysPath, UdevDevice*(const char* sys_path));
  MOCK_METHOD2(CreateDeviceFromDeviceNumber,
               UdevDevice*(char type, dev_t device_number));
  MOCK_METHOD2(CreateDeviceFromSubsystemSysName,
               UdevDevice*(const char* subsystem, const char* sys_name));
  MOCK_METHOD1(CreateMonitorFromNetlink, UdevMonitor*(const char* name));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockUdev);
};

}  // namespace mist

#endif  // MIST_MOCK_UDEV_H_
