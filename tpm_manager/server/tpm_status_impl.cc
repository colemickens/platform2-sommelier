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

#include <algorithm>
#include <vector>

#include <base/logging.h>
#include <tpm_manager/server/tpm_util.h>
#include <trousers/tss.h>
#include <trousers/trousers.h>  // NOLINT(build/include_alpha)

namespace {

// Minimum size of TPM_DA_INFO struct.
constexpr size_t kMinimumDaInfoSize = 21;

// Minimum size of TPM_CAP_VERSION_INFO struct.
constexpr size_t kMinimumVersionInfoSize = 17;

// The TPM manufacturer code of Infineon.
constexpr uint32_t kInfineonManufacturerCode = 0x49465800;

// The Infineon-specific DA info sub-capability flag.
constexpr uint32_t kInfineonMfrSubCapability = 0x00000802;

// The offset of DA counter in the Infineon-specific DA info data.
constexpr size_t kInfineonDACounterOffset = 9;

}  // namespace

namespace tpm_manager {

TpmStatusImpl::TpmStatusImpl(
    const OwnershipTakenCallBack& ownership_taken_callback)
    : ownership_taken_callback_(ownership_taken_callback) {}

bool TpmStatusImpl::IsTpmEnabled() {
  if (!is_enable_initialized_) {
    RefreshOwnedEnabledInfo();
  }
  return is_enabled_;
}

bool TpmStatusImpl::CheckAndNotifyIfTpmOwned(
    TpmStatus::TpmOwnershipStatus* status) {
  if (kTpmOwned == ownership_status_) {
    *status = ownership_status_;
    return true;
  }

  if (!is_owned_) {
    // update is_owned_
    RefreshOwnedEnabledInfo();
  }

  if (!is_owned_) {
    // We even haven't tried to take ownership yet.
    ownership_status_ = kTpmUnowned;
    *status = ownership_status_;
    return true;
  }

  ownership_status_ =
      TestTpmWithDefaultOwnerPassword() ? kTpmPreOwned : kTpmOwned;

  if (kTpmOwned == ownership_status_ && !ownership_taken_callback_.is_null()) {
    // Sends out the ownership taken signal when the value of
    // is_fully_initialized_ changes from false to true.
    ownership_taken_callback_.Run();
    ownership_taken_callback_.Reset();
  }

  *status = ownership_status_;
  return true;
}

bool TpmStatusImpl::GetDictionaryAttackInfo(uint32_t* counter,
                                            uint32_t* threshold,
                                            bool* lockout,
                                            uint32_t* seconds_remaining) {
  std::vector<uint8_t> capability_data;
  if (!GetCapability(TSS_TPMCAP_DA_LOGIC, TPM_ET_KEYHANDLE, &capability_data,
                     nullptr) ||
      capability_data.size() < kMinimumDaInfoSize) {
    LOG(ERROR) << "Error getting tpm capability data for DA info.";
    return false;
  }
  if (static_cast<uint16_t>(capability_data[1]) == TPM_TAG_DA_INFO) {
    TPM_DA_INFO da_info;
    uint64_t offset = 0;
    Trspi_UnloadBlob_DA_INFO(&offset, capability_data.data(), &da_info);
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

  // For Infineon, pulls the counter out of vendor-specific data and checks if
  // it matches the value in DA_INFO.

  if (!GetCapability(TSS_TPMCAP_PROPERTY, TSS_TPMCAP_PROP_MANUFACTURER,
                     &capability_data, nullptr) ||
                     capability_data.size() != sizeof(uint32_t)) {
    LOG(WARNING) << "Failed to query TSS_TPMCAP_PROP_MANUFACTURER. "
                    "Using the DA info from TSS_TPMCAP_DA_LOGIC.";
    return true;
  }

  uint32_t manufacturer;
  uint64_t offset = 0;
  Trspi_UnloadBlob_UINT32(&offset, &manufacturer, capability_data.data());
  if (manufacturer != kInfineonManufacturerCode) {
    return true;
  }

  if (!GetCapability(TSS_TPMCAP_MFR,
                     kInfineonMfrSubCapability,
                     &capability_data,
                     nullptr)) {
    LOG(WARNING) << "Failed to query Infineon MFR capability. "
                    "Using the DA info from TSS_TPMCAP_DA_LOGIC.";
    return true;
  }

  if (capability_data.size() <= kInfineonDACounterOffset) {
    LOG(WARNING) << "Couldn't read DA counter from Infineon's MFR "
                    "capability. Using the DA info from TSS_TPMCAP_DA_LOGIC.";
    return true;
  }

  uint32_t vendor_da_counter =
      static_cast<uint32_t>(capability_data[kInfineonDACounterOffset]);
  if (*counter != vendor_da_counter) {
    LOG(WARNING) << "DA counter mismatch for Infineon: " << *counter
                 << " vs. " << vendor_da_counter << ". Using the larger one.";
    *counter = std::max(*counter, vendor_da_counter);
  }
  return true;
}

bool TpmStatusImpl::GetVersionInfo(uint32_t* family,
                                   uint64_t* spec_level,
                                   uint32_t* manufacturer,
                                   uint32_t* tpm_model,
                                   uint64_t* firmware_version,
                                   std::vector<uint8_t>* vendor_specific) {
  std::vector<uint8_t> capability_data;
  if (!GetCapability(TSS_TPMCAP_VERSION_VAL, 0, &capability_data, nullptr) ||
      capability_data.size() < kMinimumVersionInfoSize ||
      static_cast<uint16_t>(capability_data[1]) != TPM_TAG_CAP_VERSION_INFO) {
    LOG(ERROR) << "Error getting TPM version capability data.";
    return false;
  }

  TPM_CAP_VERSION_INFO tpm_version;
  uint64_t offset = 0;
  Trspi_UnloadBlob_CAP_VERSION_INFO(
      &offset, capability_data.data(), &tpm_version);
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
  std::vector<uint8_t> capability_data;
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
                                  std::vector<uint8_t>* data,
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
