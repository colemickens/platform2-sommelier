// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tpm_softclear_utils/tpm_impl.h"

#include <string>

#include <base/logging.h>
#include <base/optional.h>

namespace tpm_softclear_utils {

base::Optional<std::string> TpmImpl::GetAuthForOwnerReset() {
  // TODO(b/134989278): add implementation.
  return std::string();
}

bool TpmImpl::SoftClearOwner(const std::string& auth_for_owner_reset) {
  LOG(INFO) << "Start soft-clearing TPM 1.2";

  // TODO(b/134991278): add implementation.
  return true;
}

bool TpmImpl::ResetOwnerPassword(const std::string& owner_auth) {
  // TODO(b/134991278): add implementation.
  return true;
}

bool TpmImpl::RemoveNvSpace() {
  // TODO(b/134991278): add implementation.
  return true;
}

bool TpmImpl::UnloadKeys() {
  // TODO(b/134991278): add implementation.
  return true;
}

bool TpmImpl::ResetDictionaryAttackCounter() {
  // TODO(b/134991278): add implementation.
  return true;
}

}  // namespace tpm_softclear_utils
