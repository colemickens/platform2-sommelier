// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_BOOTLOCKBOX_MOCK_NVRAM_BOOT_LOCKBOX_H_
#define CRYPTOHOME_BOOTLOCKBOX_MOCK_NVRAM_BOOT_LOCKBOX_H_

#include <string>

#include "cryptohome/bootlockbox/nvram_boot_lockbox.h"
#include "cryptohome/bootlockbox/tpm_nvspace_interface.h"

#include <gmock/gmock.h>

namespace cryptohome {

class MockNVRamBootLockbox : public NVRamBootLockbox {
 public:
  MockNVRamBootLockbox() : NVRamBootLockbox(NULL) {}
  virtual ~MockNVRamBootLockbox() {}

  MOCK_METHOD2(Store, bool(const std::string&, const std::string&));
  MOCK_METHOD2(Read, bool(const std::string&, std::string*));
  MOCK_METHOD0(Finalize, bool());
  MOCK_METHOD0(GetState, NVSpaceState());
  MOCK_METHOD0(DefineSpace, bool());
  MOCK_METHOD0(Load, bool());
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_BOOTLOCKBOX_MOCK_NVRAM_BOOT_LOCKBOX_H_
