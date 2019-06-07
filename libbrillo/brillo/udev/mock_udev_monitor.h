// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBBRILLO_BRILLO_UDEV_MOCK_UDEV_MONITOR_H_
#define LIBBRILLO_BRILLO_UDEV_MOCK_UDEV_MONITOR_H_

#include <memory>

#include <brillo/brillo_export.h>
#include <brillo/udev/udev_monitor.h>
#include <gmock/gmock.h>

namespace brillo {

class BRILLO_EXPORT MockUdevMonitor : public UdevMonitor {
 public:
  MockUdevMonitor() = default;
  ~MockUdevMonitor() override = default;

  MOCK_METHOD0(EnableReceiving, bool());
  MOCK_METHOD1(SetReceiveBufferSize, bool(int size));
  MOCK_CONST_METHOD0(GetFileDescriptor, int());
  MOCK_METHOD0(ReceiveDevice, std::unique_ptr<UdevDevice>());
  MOCK_METHOD2(FilterAddMatchSubsystemDeviceType,
               bool(const char* subsystem, const char* device_type));
  MOCK_METHOD1(FilterAddMatchTag, bool(const char* tag));
  MOCK_METHOD0(FilterUpdate, bool());
  MOCK_METHOD0(FilterRemove, bool());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockUdevMonitor);
};

}  // namespace brillo

#endif  // LIBBRILLO_BRILLO_UDEV_MOCK_UDEV_MONITOR_H_
