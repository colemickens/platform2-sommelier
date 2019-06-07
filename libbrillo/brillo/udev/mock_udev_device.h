// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBBRILLO_BRILLO_UDEV_MOCK_UDEV_DEVICE_H_
#define LIBBRILLO_BRILLO_UDEV_MOCK_UDEV_DEVICE_H_

#include <memory>

#include <brillo/brillo_export.h>
#include <brillo/udev/udev_device.h>
#include <gmock/gmock.h>

namespace brillo {

class BRILLO_EXPORT MockUdevDevice : public UdevDevice {
 public:
  MockUdevDevice() = default;
  ~MockUdevDevice() override = default;

  MOCK_CONST_METHOD0(GetParent, std::unique_ptr<UdevDevice>());
  MOCK_CONST_METHOD2(GetParentWithSubsystemDeviceType,
                     std::unique_ptr<UdevDevice>(const char* subsystem,
                                                 const char* device_type));
  MOCK_CONST_METHOD0(IsInitialized, bool());
  MOCK_CONST_METHOD0(GetMicrosecondsSinceInitialized, uint64_t());
  MOCK_CONST_METHOD0(GetSequenceNumber, uint64_t());
  MOCK_CONST_METHOD0(GetDevicePath, const char*());
  MOCK_CONST_METHOD0(GetDeviceNode, const char*());
  MOCK_CONST_METHOD0(GetDeviceNumber, dev_t());
  MOCK_CONST_METHOD0(GetDeviceType, const char*());
  MOCK_CONST_METHOD0(GetDriver, const char*());
  MOCK_CONST_METHOD0(GetSubsystem, const char*());
  MOCK_CONST_METHOD0(GetSysPath, const char*());
  MOCK_CONST_METHOD0(GetSysName, const char*());
  MOCK_CONST_METHOD0(GetSysNumber, const char*());
  MOCK_CONST_METHOD0(GetAction, const char*());
  MOCK_CONST_METHOD0(GetDeviceLinksListEntry, std::unique_ptr<UdevListEntry>());
  MOCK_CONST_METHOD0(GetPropertiesListEntry, std::unique_ptr<UdevListEntry>());
  MOCK_CONST_METHOD1(GetPropertyValue, const char*(const char* key));
  MOCK_CONST_METHOD0(GetTagsListEntry, std::unique_ptr<UdevListEntry>());
  MOCK_CONST_METHOD0(GetSysAttributeListEntry,
                     std::unique_ptr<UdevListEntry>());
  MOCK_CONST_METHOD1(GetSysAttributeValue, const char*(const char* attribute));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockUdevDevice);
};

}  // namespace brillo

#endif  // LIBBRILLO_BRILLO_UDEV_MOCK_UDEV_DEVICE_H_
