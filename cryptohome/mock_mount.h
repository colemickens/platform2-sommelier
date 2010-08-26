// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_MOCK_MOUNT_H_
#define CRYPTOHOME_MOCK_MOUNT_H_

#include "mount.h"

#include <gmock/gmock.h>

namespace cryptohome {
class Credentials;

class MockMount : public Mount {
 public:
  MockMount() {}
  ~MockMount() {}
  MOCK_METHOD0(Init, bool());
  MOCK_CONST_METHOD1(TestCredentials, bool(const Credentials&));
  MOCK_CONST_METHOD2(MountCryptohome, bool(const Credentials&, MountError*));
  MOCK_CONST_METHOD0(UnmountCryptohome, bool());
  MOCK_CONST_METHOD0(MountGuestCryptohome, bool());
  MOCK_CONST_METHOD2(MigratePasskey, bool(const Credentials&, const char*));
};
}  // namespace cryptohome

#endif  // CRYPTOHOME_MOCK_MOUNT_H_
