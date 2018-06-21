// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_BOOTLOCKBOX_FAKE_TPM_NVSPACE_UTILITY_H_
#define CRYPTOHOME_BOOTLOCKBOX_FAKE_TPM_NVSPACE_UTILITY_H_

#include <string>

#include <cryptohome/bootlockbox/tpm_nvspace_interface.h>

namespace cryptohome {

class FakeTpmNVSpaceUtility : public TPMNVSpaceUtilityInterface {
 public:
  FakeTpmNVSpaceUtility() {}

  bool Initialize() override;

  bool DefineNVSpace() override;

  bool DefineNVSpaceBeforeOwned() override;

  bool WriteNVSpace(const std::string& digest) override;

  bool ReadNVSpace(std::string* digest, NVSpaceState* state) override;

  bool LockNVSpace() override;

  void SetDigest(const std::string& digest);

 private:
  std::string digest_;
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_BOOTLOCKBOX_FAKE_TPM_NVSPACE_UTILITY_H_
