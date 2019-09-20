// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_BOOTLOCKBOX_MOCK_BOOT_LOCKBOX_H_
#define CRYPTOHOME_BOOTLOCKBOX_MOCK_BOOT_LOCKBOX_H_

#include "cryptohome/bootlockbox/boot_lockbox.h"

#include <brillo/secure_blob.h>
#include <gmock/gmock.h>

namespace cryptohome {

class MockBootLockbox : public BootLockbox {
 public:
  MockBootLockbox() : BootLockbox(NULL, NULL, NULL) {}
  virtual ~MockBootLockbox() {}

  MOCK_METHOD(bool,
              Sign,
              (const brillo::Blob&, brillo::SecureBlob*),
              (override));
  MOCK_METHOD(bool,
              Verify,
              (const brillo::Blob&, const brillo::SecureBlob&),
              (override));
  MOCK_METHOD(bool, FinalizeBoot, (), (override));
  MOCK_METHOD(bool, IsFinalized, (), (override));
  MOCK_METHOD(bool, PreLoadKey, (), (override));
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_BOOTLOCKBOX_MOCK_BOOT_LOCKBOX_H_
