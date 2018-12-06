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
  MOCK_METHOD1(Reset, bool(LockboxError*));
  MOCK_METHOD2(Store, bool(const brillo::Blob&, LockboxError*));

  MOCK_METHOD1(set_tpm, void(Tpm*));
  MOCK_METHOD0(tpm, Tpm*());

  MOCK_METHOD1(set_platform, void(Platform*));
  MOCK_METHOD0(platform, Platform*());
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_MOCK_LOCKBOX_H_
