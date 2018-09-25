// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_PENDING_ACTIVATION_STORE_H_
#define SHILL_MOCK_PENDING_ACTIVATION_STORE_H_

#include <string>

#include <base/files/file_path.h>
#include <gmock/gmock.h>

#include "shill/pending_activation_store.h"

namespace shill {

class MockPendingActivationStore : public PendingActivationStore {
 public:
  MockPendingActivationStore();
  ~MockPendingActivationStore() override;

  MOCK_METHOD1(InitStorage, bool(const base::FilePath& storage_path));
  MOCK_CONST_METHOD2(GetActivationState,
                     State(IdentifierType type, const std::string& iccid));
  MOCK_METHOD3(SetActivationState,
               bool(IdentifierType type,
                    const std::string& iccid,
                    State state));
  MOCK_METHOD2(RemoveEntry,
               bool(IdentifierType type, const std::string& iccid));
};

}  // namespace shill

#endif  // SHILL_MOCK_PENDING_ACTIVATION_STORE_H_
