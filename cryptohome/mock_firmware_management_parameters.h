// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_MOCK_FIRMWARE_MANAGEMENT_PARAMETERS_H_
#define CRYPTOHOME_MOCK_FIRMWARE_MANAGEMENT_PARAMETERS_H_

#include "cryptohome/firmware_management_parameters.h"
#include <gmock/gmock.h>

namespace cryptohome {
class MockFirmwareManagementParameters : public FirmwareManagementParameters {
 public:
  MockFirmwareManagementParameters();
  virtual ~MockFirmwareManagementParameters();

  MOCK_METHOD0(Create, bool());
  MOCK_METHOD0(Destroy, bool());
  MOCK_METHOD0(Load, bool());
  MOCK_METHOD2(Store, bool(uint32_t, const brillo::Blob*));
  MOCK_METHOD1(GetFlags, bool(uint32_t*));
  MOCK_METHOD1(GetDeveloperKeyHash, bool(brillo::Blob*));
  MOCK_CONST_METHOD0(IsLoaded, bool());

  // Formerly protected methods.
  MOCK_CONST_METHOD0(HasAuthorization, bool());
  MOCK_CONST_METHOD0(TpmIsReady, bool());
};
}  // namespace cryptohome

#endif  // CRYPTOHOME_MOCK_FIRMWARE_MANAGEMENT_PARAMETERS_H_
