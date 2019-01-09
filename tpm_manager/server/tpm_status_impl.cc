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

#include "tpm_manager/server/tpm_status_impl.h"

#include <vector>

#include <base/logging.h>
#include <tpm_manager/server/tpm_util.h>
#include <trousers/tss.h>
#include <trousers/trousers.h>  // NOLINT(build/include_alpha)

namespace tpm_manager {

// Minimum size of TPM_DA_INFO struct.
const size_t kMinimumDaInfoSize = 21;
// Minimum size of TPM_CAP_VERSION_INFO struct.
const size_t kMinimumVersionInfoSize = 17;

TpmStatusImpl::TpmStatusImpl(
    const OwnershipTakenCallBack& ownership_taken_callback)
    : ownership_taken_callback_(ownership_taken_callback) {}

bool TpmStatusImpl::IsTpmEnabled() {
  if (!is_enable_initialized_) {
    RefreshOwnedEnabledInfo();
  }
  return is_enabled_;
}

TpmStatus::TpmOwnershipStatus TpmStatusImpl::CheckAndNotifyIfTpmOwned() {
  if (kTpmOwned == ownership_status_) {
    return ownership_status_;
  }

  if (!is_owned_) {
    // update is_owned_
    RefreshOwnedEnabledInfo();
  }

  if (!is_owned_) {
    // We even haven't tried to take ownership yet.
    ownership_status_ = kTpmUnowned;
    return ownership_status_;
  }

  ownership_status_ =
      TestTpmWithDefaultOwnerPassword() ? kTpmPreOwned : kTpmOwned;

  if (kTpmOwned == ownership_status_ && !ownership_taken_callback_.is_null()) {
    // Sends out the ownership taken signal when the value of
    // is_fully_initialized_ changes from false to true.
    ownership_taken_callback_.Run();
    ownership_taken_callback_.Reset();
  }

  return ownership_status_;
}

bool TpmStatusImpl::GetDictionaryAttackInfo(int* counter,
                                            int* threshold,
                                            bool* lockout,
                                            int* seconds_remaining) {
  std::string capability_data;
  if (!GetCapability(TSS_TPMCAP_DA_LOGIC, TPM_ET_KEYHANDLE, &capability_data,
                     nullptr) ||
      capability_data.size() < kMinimumDaInfoSize) {
    LOG(ERROR) << "Error getting tpm capability data.";
    return false;
  }
  if (static_cast<uint16_t>(capability_data[1]) == TPM_TAG_DA_INFO) {
    TPM_DA_INFO da_info;
    uint64_t offset = 0;
    std::vector<BYTE> bytes(capability_data.begin(), capability_data.end());
    Trspi_UnloadBlob_DA_INFO(&offset, bytes.data(), &da_info);
    if (counter) {
      *counter = da_info.currentCount;
    }
    if (threshold) {
      *threshold = da_info.thresholdCount;
    }
    if (lockout) {
      *lockout = (da_info.state == TPM_DA_STATE_ACTIVE);
    }
    if (seconds_remaining) {
      *seconds_remaining = da_info.actionDependValue;
    }
  }
  return true;
}

bool TpmStatusImpl::GetVersionInfo(uint32_t* family,
                                   uint64_t* spec_level,
                                   uint32_t* manufacturer,
                                   uint32_t* tpm_model,
                                   uint64_t* firmware_version,
                                   std::vector<uint8_t>* vendor_specific) {
  std::string capability_data;
  if (!GetCapability(TSS_TPMCAP_VERSION_VAL, 0, &capability_data, nullptr) ||
      capability_data.size() < kMinimumVersionInfoSize ||
      static_cast<uint16_t>(capability_data[1]) != TPM_TAG_CAP_VERSION_INFO) {
    LOG(ERROR) << "Error getting TPM version capability data.";
    return false;
  }

  TPM_CAP_VERSION_INFO tpm_version;
  uint64_t offset = 0;
  std::vector<BYTE> bytes(capability_data.begin(), capability_data.end());
  Trspi_UnloadBlob_CAP_VERSION_INFO(&offset, bytes.data(), &tpm_version);
  if (family) {
    *family = 0x312e3200;
  }
  if (spec_level) {
    *spec_level = (static_cast<uint64_t>(tpm_version.specLevel) << 32) |
                  tpm_version.errataRev;
  }
  if (manufacturer) {
    *manufacturer = (tpm_version.tpmVendorID[0] << 24) |
                    (tpm_version.tpmVendorID[1] << 16) |
                    (tpm_version.tpmVendorID[2] << 8) |
                    (tpm_version.tpmVendorID[3] << 0);
  }
  if (tpm_model) {
    // There's no generic model field in the spec. Model information might be
    // present in the vendor-specific data returned by CAP_VERSION_INFO, so if
    // we ever require to know the model, we'll need to check with hardware
    // vendors for the best way to determine it.
    *tpm_model = ~0;
  }
  if (firmware_version) {
    *firmware_version =
        (tpm_version.version.revMajor << 8) | tpm_version.version.revMinor;
  }
  if (vendor_specific) {
    const uint8_t* data =
        reinterpret_cast<const uint8_t*>(tpm_version.vendorSpecific);
    vendor_specific->assign(data, data + tpm_version.vendorSpecificSize);
  }
  free(tpm_version.vendorSpecific);
  return true;
}

bool TpmStatusImpl::TestTpmWithDefaultOwnerPassword() {
  if (!is_owner_password_state_dirty_) {
    return is_owner_password_default_;
  }

  TpmConnection connection(GetDefaultOwnerPassword());
  TSS_HTPM tpm_handle = connection.GetTpm();
  if (tpm_handle == 0) {
    return false;
  }

  // Call Tspi_TPM_GetStatus to test the default owner password.
  TSS_BOOL current_status = false;
  TSS_RESULT result = Tspi_TPM_GetStatus(
      tpm_handle, TSS_TPMSTATUS_DISABLED, &current_status);

  // TODO(garryxiao): tell the difference between invalid owner password and TPM
  // communication errors.
  is_owner_password_default_ = !TPM_ERROR(result);
  is_owner_password_state_dirty_ = false;

  return is_owner_password_default_;
}

void TpmStatusImpl::RefreshOwnedEnabledInfo() {
  TSS_RESULT result;
  std::string capability_data;
  if (!GetCapability(TSS_TPMCAP_PROPERTY, TSS_TPMCAP_PROP_OWNER,
                     &capability_data, &result)) {
    if (ERROR_CODE(result) == TPM_E_DISABLED) {
      is_enable_initialized_ = true;
      is_enabled_ = false;
    }
  } else {
    is_enable_initialized_ = true;
    is_enabled_ = true;
    // |capability_data| should be populated with a TSS_BOOL which is true iff
    // the Tpm is owned.
    if (capability_data.size() != sizeof(TSS_BOOL)) {
      LOG(ERROR) << "Error refreshing Tpm ownership information.";
      return;
    }
    is_owned_ = (capability_data[0] != 0);
  }
}

bool TpmStatusImpl::GetCapability(uint32_t capability,
                                  uint32_t sub_capability,
                                  std::string* data,
                                  TSS_RESULT* tpm_result) {
  CHECK(data);
  TSS_HTPM tpm_handle = tpm_connection_.GetTpm();
  if (tpm_handle == 0) {
    return false;
  }
  uint32_t length = 0;
  trousers::ScopedTssMemory buf(tpm_connection_.GetContext());
  TSS_RESULT result = Tspi_TPM_GetCapability(
      tpm_handle, capability, sizeof(uint32_t),
      reinterpret_cast<BYTE*>(&sub_capability), &length, buf.ptr());
  if (tpm_result) {
    *tpm_result = result;
  }
  if (TPM_ERROR(result)) {
    LOG(ERROR) << "Error getting TPM capability data.";
    return false;
  }
  data->assign(buf.value(), buf.value() + length);
  return true;
}

}  // namespace tpm_manager
