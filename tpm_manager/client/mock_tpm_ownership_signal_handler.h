// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TPM_MANAGER_CLIENT_MOCK_TPM_OWNERSHIP_SIGNAL_HANDLER_H_
#define TPM_MANAGER_CLIENT_MOCK_TPM_OWNERSHIP_SIGNAL_HANDLER_H_

#include "tpm_manager/client/tpm_ownership_signal_handler.h"

#include <string>

#include <gmock/gmock.h>

namespace tpm_manager {

class MockTpmOwnershipTakenSignalHandler
    : public TpmOwnershipTakenSignalHandler {
 public:
  MockTpmOwnershipTakenSignalHandler() = default;
  ~MockTpmOwnershipTakenSignalHandler() override = default;
  MOCK_METHOD(void,
              OnOwnershipTaken,
              (const OwnershipTakenSignal&),
              (override));
  MOCK_METHOD(void,
              OnSignalConnected,
              (const std::string&, const std::string&, bool),
              (override));
};

}  // namespace tpm_manager

#endif  // TPM_MANAGER_CLIENT_MOCK_TPM_OWNERSHIP_SIGNAL_HANDLER_H_
