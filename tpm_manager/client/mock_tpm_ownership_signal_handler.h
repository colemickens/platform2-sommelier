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
  virtual ~MockTpmOwnershipTakenSignalHandler() = default;
  MOCK_METHOD1(OnOwnershipTaken, void(const OwnershipTakenSignal&));
  MOCK_METHOD3(OnSignalConnected,
               void(const std::string&, const std::string&, bool));
};

}  // namespace tpm_manager

#endif  // TPM_MANAGER_CLIENT_MOCK_TPM_OWNERSHIP_SIGNAL_HANDLER_H_
