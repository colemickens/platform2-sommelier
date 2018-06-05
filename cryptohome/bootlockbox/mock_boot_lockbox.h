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

  MOCK_METHOD2(Sign, bool(const brillo::Blob&, brillo::SecureBlob*));
  MOCK_METHOD2(Verify, bool(const brillo::Blob&,
                            const brillo::SecureBlob&));
  MOCK_METHOD0(FinalizeBoot, bool());
  MOCK_METHOD0(IsFinalized, bool());
  MOCK_METHOD0(PreLoadKey, bool());
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_BOOTLOCKBOX_MOCK_BOOT_LOCKBOX_H_
