// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tpm_manager/server/tpm2_status_impl.h"

#include <base/logging.h>
#include <trunks/error_codes.h>
#include <trunks/tpm_generated.h>
#include <trunks/trunks_factory_impl.h>

using trunks::TPM_RC;
using trunks::TPM_RC_SUCCESS;

namespace tpm_manager {

Tpm2StatusImpl::Tpm2StatusImpl()
  : default_trunks_factory_(new trunks::TrunksFactoryImpl()),
    trunks_factory_(default_trunks_factory_.get()),
    trunks_tpm_state_(trunks_factory_->GetTpmState()) {}

Tpm2StatusImpl::Tpm2StatusImpl(trunks::TrunksFactory* factory)
  : trunks_factory_(factory),
    trunks_tpm_state_(trunks_factory_->GetTpmState()) {}

bool Tpm2StatusImpl::IsTpmEnabled() {
  if (!initialized_) {
    Refresh();
  }
  return trunks_tpm_state_->IsEnabled();
}

bool Tpm2StatusImpl::IsTpmOwned() {
  if (!is_owned_) {
    Refresh();
  }
  is_owned_ = trunks_tpm_state_->IsOwned();
  return is_owned_;
}

bool Tpm2StatusImpl::GetDictionaryAttackInfo(int* counter,
                                             int* threshold,
                                             bool* lockout,
                                             int* seconds_remaining) {
  if (!Refresh()) {
    return false;
  }
  if (counter) {
    *counter = trunks_tpm_state_->GetLockoutCounter();
  }
  if (threshold) {
    *threshold = trunks_tpm_state_->GetLockoutThreshold();
  }
  if (lockout) {
    *lockout = trunks_tpm_state_->IsInLockout();
  }
  if (seconds_remaining) {
    *seconds_remaining = trunks_tpm_state_->GetLockoutCounter() *
                         trunks_tpm_state_->GetLockoutInterval();
  }
  return true;
}

bool Tpm2StatusImpl::Refresh() {
  TPM_RC result = trunks_tpm_state_->Initialize();
  if (result != TPM_RC_SUCCESS) {
    LOG(WARNING) << "Error initializing trunks tpm state: "
                 << trunks::GetErrorString(result);
    return false;
  }
  initialized_ = true;
  return true;
}

}  // namespace tpm_manager
