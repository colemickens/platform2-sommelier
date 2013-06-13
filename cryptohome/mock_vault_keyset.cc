// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mock_vault_keyset.h"

namespace cryptohome {

using ::testing::Invoke;

MockVaultKeyset::MockVaultKeyset() {
  ON_CALL(*this, serialized())
      .WillByDefault(
        Invoke(this,
          &MockVaultKeyset::StubSerialized));
}

MockVaultKeyset::~MockVaultKeyset() {}

}  // namespace cryptohome
