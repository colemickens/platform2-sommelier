// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_MOCK_LOCKBOX_H_
#define CRYPTOHOME_MOCK_LOCKBOX_H_

#include "lockbox.h"

#include <base/logging.h>
#include <chromeos/utility.h>

#include <gmock/gmock.h>

namespace cryptohome {
using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

class MockLockbox : public Lockbox {
 public:
  MockLockbox() : Lockbox(NULL, 0) {}
  virtual ~MockLockbox() {}
  MOCK_METHOD1(Create, bool(ErrorId*));
  MOCK_METHOD1(Destroy, bool(ErrorId*));
  MOCK_METHOD1(Load, bool(ErrorId*));
  MOCK_METHOD2(Verify, bool(const chromeos::Blob&, ErrorId*));
  MOCK_METHOD2(Store, bool(const chromeos::Blob&, ErrorId*));

  MOCK_METHOD1(set_tpm, void(Tpm*));
  MOCK_METHOD0(tpm, Tpm*());

  MOCK_METHOD1(set_crypto, void(Crypto*));
  MOCK_METHOD0(crypto, Crypto*());

  MOCK_METHOD1(set_platform, void(Platform*));
  MOCK_METHOD0(platform, Platform*());

  MOCK_METHOD1(SetDataDirectory, void(const char*));
  MOCK_METHOD0(path, const char*());

  // Formerly protected methods.
  MOCK_CONST_METHOD0(HasAuthorization, bool());
  MOCK_CONST_METHOD0(TpmIsReady, bool());
  MOCK_CONST_METHOD2(GetSizeBlob, bool(const chromeos::Blob&, chromeos::Blob*));
  MOCK_CONST_METHOD2(ParseSizeBlob, bool(const chromeos::Blob&, uint32_t*));
};
}  // namespace cryptohome

#endif  // CRYPTOHOME_MOCK_LOCKBOX_H_
