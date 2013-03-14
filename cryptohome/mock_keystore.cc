// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mock_keystore.h"

namespace cryptohome {

MockKeyStore::MockKeyStore() {
  ON_CALL(*this, Read(_, _)).WillByDefault(Return(true));
  ON_CALL(*this, Write(_, _)).WillByDefault(Return(true));
  ON_CALL(*this, Delete(_)).WillByDefault(Return(true));
  ON_CALL(*this, Register(_, _)).WillByDefault(Return(true));
}

MockKeyStore::~MockKeyStore() {}

}  // namespace cryptohome
