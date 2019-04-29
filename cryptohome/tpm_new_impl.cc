// Copyright (c) 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/tpm_new_impl.h"

namespace cryptohome {

bool TpmNewImpl::GetOwnerPassword(brillo::SecureBlob* owner_password) {
  if (IsOwned()) {
    *owner_password =
        brillo::SecureBlob(last_tpm_manager_data_.owner_password());
    if (owner_password->empty()) {
      LOG(WARNING) << __func__
                   << ": Trying to get owner password after it is cleared.";
    }
  } else {
    LOG(ERROR)
        << __func__
        << ": Cannot get owner password until TPM is confirmed to be owned.";
    owner_password->clear();
  }
  return !owner_password->empty();
}

bool TpmNewImpl::InitializeTpmManagerUtility() {
  return tpm_manager_utility_.Initialize();
}

bool TpmNewImpl::CacheTpmManagerStatus() {
  if (!InitializeTpmManagerUtility()) {
    LOG(ERROR) << __func__ << ": Failed to initialize |TpmMangerUtility|.";
    return false;
  }
  return tpm_manager_utility_.GetTpmStatus(&is_enabled_, &is_owned_,
                                           &last_tpm_manager_data_);
}

bool TpmNewImpl::IsEnabled() {
  if (!is_enabled_) {
    if (!CacheTpmManagerStatus()) {
      LOG(ERROR) << __func__
                 << ": Failed to update TPM status from tpm manager.";
      return false;
    }
  }
  return is_enabled_;
}

bool TpmNewImpl::IsOwned() {
  if (!is_owned_) {
    if (!CacheTpmManagerStatus()) {
      LOG(ERROR) << __func__
                 << ": Failed to update TPM status from tpm manager.";
      return false;
    }
  }
  return is_owned_;
}

bool TpmNewImpl::TakeOwnership(int, const brillo::SecureBlob&) {
  if (!InitializeTpmManagerUtility()) {
    LOG(ERROR) << __func__ << ": Failed to initialize |TpmMangerUtility|.";
    return false;
  }
  if (IsOwned()) {
    LOG(INFO) << __func__ << ": TPM is already owned.";
    return true;
  }
  return tpm_manager_utility_.TakeOwnership();
}

void TpmNewImpl::SetOwnerPassword(const brillo::SecureBlob&) {
  LOG(WARNING) << __func__ << ": no-ops.";
}

void TpmNewImpl::SetIsEnabled(bool) {
  LOG(WARNING) << __func__ << ": no-ops.";
}

void TpmNewImpl::SetIsOwned(bool) {
  LOG(WARNING) << __func__ << ": no-ops.";
}

bool TpmNewImpl::GetDelegate(brillo::Blob* blob,
                             brillo::Blob* secret,
                             bool* has_reset_lock_permissions) {
  blob->clear();
  secret->clear();
  if (last_tpm_manager_data_.owner_delegate().blob().empty() ||
      last_tpm_manager_data_.owner_delegate().secret().empty()) {
    if (!CacheTpmManagerStatus()) {
      LOG(ERROR) << __func__
                 << ": Failed to update TPM status from tpm manager.";
      return false;
    }
  }
  const auto& owner_delegate = last_tpm_manager_data_.owner_delegate();
  *blob = brillo::BlobFromString(owner_delegate.blob());
  *secret = brillo::BlobFromString(owner_delegate.secret());
  *has_reset_lock_permissions = owner_delegate.has_reset_lock_permissions();
  return !blob->empty() && !secret->empty();
}

bool TpmNewImpl::DoesUseTpmManager() {
  return true;
}

bool TpmNewImpl::GetDictionaryAttackInfo(int* counter,
                                         int* threshold,
                                         bool* lockout,
                                         int* seconds_remaining) {
  if (!InitializeTpmManagerUtility()) {
    LOG(ERROR) << __func__ << ": failed to initialize |TpmMangerUtility|.";
    return false;
  }
  return tpm_manager_utility_.GetDictionaryAttackInfo(
      counter, threshold, lockout, seconds_remaining);
}

bool TpmNewImpl::ResetDictionaryAttackMitigation(const brillo::Blob&,
                                                 const brillo::Blob&) {
  if (!InitializeTpmManagerUtility()) {
    LOG(ERROR) << __func__ << ": failed to initialize |TpmMangerUtility|.";
    return false;
  }
  return tpm_manager_utility_.ResetDictionaryAttackLock();
}

}  // namespace cryptohome
