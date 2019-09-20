// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_MOCK_LOCKBOX_H_
#define CRYPTOHOME_MOCK_LOCKBOX_H_

#include "cryptohome/lockbox.h"

#include <brillo/secure_blob.h>

#include <gmock/gmock.h>

namespace cryptohome {

class MockLockbox : public Lockbox {
 public:
  MockLockbox();
  virtual ~MockLockbox();
  MOCK_METHOD(bool, Reset, (LockboxError*), (override));
  MOCK_METHOD(bool, Store, (const brillo::Blob&, LockboxError*), (override));

  MOCK_METHOD(void, set_tpm, (Tpm*), (override));
  MOCK_METHOD(Tpm*, tpm, (), (override));

  MOCK_METHOD(void, set_platform, (Platform*), (override));
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_MOCK_LOCKBOX_H_
