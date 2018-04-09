// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CECSERVICE_UDEV_MOCK_H_
#define CECSERVICE_UDEV_MOCK_H_

#include <memory>
#include <vector>

#include <base/macros.h>
#include <gmock/gmock.h>

#include "cecservice/udev.h"

namespace cecservice {

class UdevMock : public Udev {
 public:
  UdevMock() = default;

  MOCK_CONST_METHOD1(EnumerateDevices, bool(std::vector<base::FilePath>*));

 private:
  DISALLOW_COPY_AND_ASSIGN(UdevMock);
};

class UdevFactoryMock : public UdevFactory {
 public:
  UdevFactoryMock() = default;

  MOCK_CONST_METHOD2(Create,
                     std::unique_ptr<Udev>(
                         const Udev::DeviceCallback& device_added_callback,
                         const Udev::DeviceCallback& device_removed_callback));

 private:
  DISALLOW_COPY_AND_ASSIGN(UdevFactoryMock);
};

}  // namespace cecservice

#endif  // CECSERVICE_UDEV_MOCK_H_
