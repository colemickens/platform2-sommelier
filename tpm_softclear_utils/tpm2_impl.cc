// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tpm_softclear_utils/tpm2_impl.h"

#include <vector>

#include <base/logging.h>
#include <base/optional.h>

namespace tpm_softclear_utils {

base::Optional<std::vector<uint8_t>> Tpm2Impl::GetAuthForOwnerReset() {
  // TODO(b/134989277): add implementation.
  return std::vector<uint8_t>();
}

bool Tpm2Impl::SoftClearOwner(
    const std::vector<uint8_t>& auth_for_owner_reset) {
  LOG(INFO) << "Start soft-clearing TPM 2.0";

  // TODO(b/134989277): add implementation.
  return true;
}

}  // namespace tpm_softclear_utils
