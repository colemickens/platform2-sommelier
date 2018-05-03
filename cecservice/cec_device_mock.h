// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CECSERVICE_CEC_DEVICE_MOCK_H_
#define CECSERVICE_CEC_DEVICE_MOCK_H_

#include <memory>

#include <base/macros.h>
#include <gmock/gmock.h>

#include "cecservice/cec_device.h"

namespace cecservice {

class CecDeviceMock : public CecDevice {
 public:
  CecDeviceMock() = default;
  ~CecDeviceMock() override { DestructorCalled(); };

  MOCK_METHOD1(GetTvPowerStatus, void(GetTvPowerStatusCallback callback));
  MOCK_METHOD0(SetStandBy, void());
  MOCK_METHOD0(SetWakeUp, void());
  MOCK_METHOD0(DestructorCalled, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(CecDeviceMock);
};

class CecDeviceFactoryMock : public CecDeviceFactory {
 public:
  CecDeviceFactoryMock() = default;
  MOCK_CONST_METHOD1(Create,
                     std::unique_ptr<CecDevice>(const base::FilePath& path));

 private:
  DISALLOW_COPY_AND_ASSIGN(CecDeviceFactoryMock);
};

}  // namespace cecservice

#endif  // CECSERVICE_CEC_DEVICE_MOCK_H_
