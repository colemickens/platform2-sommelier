// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_MOCK_MOUNT_H_
#define CRYPTOHOME_MOCK_MOUNT_H_

#include "cryptohome/mount.h"

#include <string>

#include <base/files/file_path.h>
#include <gmock/gmock.h>

namespace cryptohome {
class Credentials;

class MockMount : public Mount {
 public:
  MockMount();
  ~MockMount();
  MOCK_METHOD4(Init, bool(Platform*, Crypto*,
                          UserOldestActivityTimestampCache*,
                          PreMountCallback pre_mount_callback));
  MOCK_METHOD1(AreSameUser, bool(const Credentials&));
  MOCK_METHOD1(AreValid, bool(const Credentials&));
  MOCK_METHOD3(MountCryptohome, bool(const Credentials&,
                                           const Mount::MountArgs&,
                                           MountError*));
  MOCK_METHOD0(UnmountCryptohome, bool());
  MOCK_CONST_METHOD0(IsMounted, bool());
  MOCK_CONST_METHOD0(IsNonEphemeralMounted, bool());
  MOCK_METHOD0(MountGuestCryptohome, bool());
  MOCK_METHOD2(MigratePasskey, bool(const Credentials&, const char*));
  MOCK_METHOD1(RemoveCryptohome, bool(const Credentials&));
  MOCK_METHOD1(UpdateCurrentUserActivityTimestamp, bool(int)); // NOLINT
  MOCK_CONST_METHOD0(mount_point, const base::FilePath&());
  MOCK_CONST_METHOD1(OwnsMountPoint, bool(const base::FilePath&));
};
}  // namespace cryptohome

#endif  // CRYPTOHOME_MOCK_MOUNT_H_
