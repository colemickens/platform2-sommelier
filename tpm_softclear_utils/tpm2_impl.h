// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TPM_SOFTCLEAR_UTILS_TPM2_IMPL_H_
#define TPM_SOFTCLEAR_UTILS_TPM2_IMPL_H_

#include "tpm_softclear_utils/tpm.h"

#include <vector>

#include <base/macros.h>
#include <base/optional.h>

namespace tpm_softclear_utils {

// Utility class for soft-clearing TPM 2.0.
class Tpm2Impl : public Tpm {
 public:
  Tpm2Impl() = default;
  ~Tpm2Impl() override = default;

  // Gets the lockout password from tpm_manager's DB and returns it. In case of
  // an error, returns an empty Optional object.
  base::Optional<std::vector<uint8_t>> GetAuthForOwnerReset() override;

  // Clears the TPM ownership, including resetting the owner hierarchy and
  // endorsement hierarchy, using the lockout password in
  // |auth_for_owner_reset|.
  //
  // Returns if the TPM is soft-cleared successfully.
  bool SoftClearOwner(
      const std::vector<uint8_t>& auth_for_owner_reset) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(Tpm2Impl);
};

}  // namespace tpm_softclear_utils

#endif  // TPM_SOFTCLEAR_UTILS_TPM2_IMPL_H_
