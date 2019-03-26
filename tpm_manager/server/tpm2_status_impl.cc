//
// Copyright (C) 2015 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "tpm_manager/server/tpm2_status_impl.h"

#include <string>

#include <base/logging.h>
#include <trunks/error_codes.h>
#include <trunks/tpm_generated.h>
#include <trunks/trunks_factory_impl.h>

using trunks::TPM_RC;
using trunks::TPM_RC_SUCCESS;

namespace tpm_manager {

Tpm2StatusImpl::Tpm2StatusImpl(
    const trunks::TrunksFactory& factory,
    const OwnershipTakenCallBack& ownership_taken_callback)
    : trunks_factory_(factory),
      trunks_tpm_state_(trunks_factory_.GetTpmState()),
      ownership_taken_callback_(ownership_taken_callback) {}

bool Tpm2StatusImpl::IsTpmEnabled() {
  // For 2.0, TPM is always enabled.
  return true;
}

TpmStatus::TpmOwnershipStatus Tpm2StatusImpl::CheckAndNotifyIfTpmOwned() {
  if (kTpmOwned == ownership_status_) {
    return kTpmOwned;
  }

  Refresh();

  if (trunks_tpm_state_->IsOwned()) {
    ownership_status_ = kTpmOwned;
  } else if (trunks_tpm_state_->IsOwnerPasswordSet()) {
    ownership_status_ = kTpmPreOwned;
  }

  if (kTpmOwned == ownership_status_ && !ownership_taken_callback_.is_null()) {
    // Sends out the ownership taken signal when the TPM ownership status
    // changes to owned from a different state.
    ownership_taken_callback_.Run();
    ownership_taken_callback_.Reset();
  }

  return ownership_status_;
}

bool Tpm2StatusImpl::GetDictionaryAttackInfo(uint32_t* counter,
                                             uint32_t* threshold,
                                             bool* lockout,
                                             uint32_t* seconds_remaining) {
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

bool Tpm2StatusImpl::GetVersionInfo(uint32_t* family,
                                    uint64_t* spec_level,
                                    uint32_t* manufacturer,
                                    uint32_t* tpm_model,
                                    uint64_t* firmware_version,
                                    std::vector<uint8_t>* vendor_specific) {
  if (!Refresh()) {
    return false;
  }

  if (family) {
    *family = trunks_tpm_state_->GetTpmFamily();
  }
  if (spec_level) {
    uint64_t level = trunks_tpm_state_->GetSpecificationLevel();
    uint64_t revision = trunks_tpm_state_->GetSpecificationRevision();
    *spec_level = (level << 32) | revision;
  }
  if (manufacturer) {
    *manufacturer = trunks_tpm_state_->GetManufacturer();
  }
  if (tpm_model) {
    *tpm_model = trunks_tpm_state_->GetTpmModel();
  }
  if (firmware_version) {
    *firmware_version = trunks_tpm_state_->GetFirmwareVersion();
  }
  if (vendor_specific) {
    std::string vendor_id_string = trunks_tpm_state_->GetVendorIDString();
    const uint8_t* data =
        reinterpret_cast<const uint8_t*>(vendor_id_string.data());
    vendor_specific->assign(data, data + vendor_id_string.size());
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
