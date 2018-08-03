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
  MOCK_METHOD1(Create, bool(LockboxError*));  // NOLINT
  MOCK_METHOD1(Destroy, bool(LockboxError*));  // NOLINT
  MOCK_METHOD1(Load, bool(LockboxError*));  // NOLINT
  MOCK_METHOD2(Verify, bool(const brillo::Blob&, LockboxError*));
  MOCK_METHOD2(Store, bool(const brillo::Blob&, LockboxError*));

  MOCK_METHOD1(set_tpm, void(Tpm*));  // NOLINT
  MOCK_METHOD0(tpm, Tpm*());

  MOCK_METHOD1(set_platform, void(Platform*));  // NOLINT
  MOCK_METHOD0(platform, Platform*());

  MOCK_METHOD1(SetDataDirectory, void(const char*));
  MOCK_METHOD0(path, const char*());

  // Formerly protected methods.
  MOCK_CONST_METHOD0(HasAuthorization, bool());
  MOCK_CONST_METHOD0(TpmIsReady, bool());
  MOCK_CONST_METHOD2(GetSizeBlob, bool(const brillo::Blob&, brillo::Blob*));
  MOCK_CONST_METHOD2(ParseSizeBlob, bool(const brillo::Blob&, uint32_t*));
};
}  // namespace cryptohome

#endif  // CRYPTOHOME_MOCK_LOCKBOX_H_
