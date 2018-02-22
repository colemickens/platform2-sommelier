// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/mock_keystore.h"

using testing::_;
using testing::Return;

namespace cryptohome {

MockKeyStore::MockKeyStore() {
  ON_CALL(*this, Read(_, _, _, _)).WillByDefault(Return(true));
  ON_CALL(*this, Write(_, _, _, _)).WillByDefault(Return(true));
  ON_CALL(*this, Delete(_, _, _)).WillByDefault(Return(true));
  ON_CALL(*this, DeleteByPrefix(_, _, _)).WillByDefault(Return(true));
  ON_CALL(*this, Register(_, _, _, _, _, _)).WillByDefault(Return(true));
  ON_CALL(*this, RegisterCertificate(_, _, _)).WillByDefault(Return(true));
}

MockKeyStore::~MockKeyStore() {}

}  // namespace cryptohome
