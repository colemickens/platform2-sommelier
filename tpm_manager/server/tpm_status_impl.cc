// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tpm_manager/server/tpm_status_impl.h"

#include <vector>

#include <base/threading/platform_thread.h>
#include <base/stl_util.h>
#include <base/time/time.h>
#include <trousers/scoped_tss_type.h>
#include <trousers/tss.h>
#include <trousers/trousers.h>  // NOLINT(build/include_alpha)

namespace tpm_manager {

#define TPM_LOG(severity, result) \
  LOG(severity) << "TPM error 0x" << std::hex << result \
                << " (" << Trspi_Error_String(result) << "): "

const unsigned int kTpmConnectRetries = 10;
const unsigned int kTpmConnectIntervalMs = 100;
// Minimum size of TPM_DA_INFO struct.
const size_t kMinimumDaInfoSize = 21;

bool TpmStatusImpl::IsTpmEnabled() {
  if (!is_enable_initialized_) {
    RefreshOwnedEnabledInfo();
  }
  return is_enabled_;
}

bool TpmStatusImpl::IsTpmOwned() {
  if (!is_owned_) {
    RefreshOwnedEnabledInfo();
  }
  return is_owned_;
}

bool TpmStatusImpl::GetDictionaryAttackInfo(int* counter,
                                            int* threshold,
                                            bool* lockout,
                                            int* seconds_remaining) {
  std::string capability_data;
  if (!GetCapability(TSS_TPMCAP_DA_LOGIC, TPM_ET_KEYHANDLE,
                     &capability_data, nullptr) ||
      capability_data.size() < kMinimumDaInfoSize) {
    LOG(ERROR) << "Error getting tpm capability data.";
    return false;
  }
  if (static_cast<uint16_t>(capability_data[1]) == TPM_TAG_DA_INFO) {
    TPM_DA_INFO da_info;
    uint64_t offset = 0;
    std::vector<BYTE> bytes(capability_data.begin(), capability_data.end());
    Trspi_UnloadBlob_DA_INFO(&offset, bytes.data(), &da_info);
    if (counter) { *counter = da_info.currentCount; }
    if (threshold) { *threshold = da_info.thresholdCount; }
    if (lockout) { *lockout = (da_info.state == TPM_DA_STATE_ACTIVE); }
    if (seconds_remaining) { *seconds_remaining = da_info.actionDependValue; }
  }
  return true;
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
  TSS_HTPM tpm_handle = GetTpm();
  if (tpm_handle == 0) {
    return false;
  }
  uint32_t length = 0;
  trousers::ScopedTssMemory buf(context_.value());
  TSS_RESULT result = Tspi_TPM_GetCapability(
      tpm_handle, capability, sizeof(uint32_t),
      reinterpret_cast<BYTE*>(&sub_capability),
      &length, buf.ptr());
  if (tpm_result) {
    *tpm_result = result;
  }
  if (TPM_ERROR(result)) {
    TPM_LOG(ERROR, result) << "Error getting TPM capability data.";
    return false;
  }
  data->assign(buf.value(), buf.value() + length);
  return true;
}

TSS_HTPM TpmStatusImpl::GetTpm() {
  if (context_.value() == 0 && !ConnectContext()) {
    LOG(ERROR) << "Cannot get a handle to the TPM without context.";
    return 0;
  }
  TSS_RESULT result;
  TSS_HTPM tpm_handle;
  if (TPM_ERROR(result = Tspi_Context_GetTpmObject(context_.value(),
                                                   &tpm_handle))) {
    TPM_LOG(ERROR, result) << "Error getting a handle to the TPM.";
    return 0;
  }
  return tpm_handle;
}

bool TpmStatusImpl::ConnectContext() {
  if (context_.value() != 0) {
    return true;
  }
  TSS_RESULT result;
  if (TPM_ERROR(result = Tspi_Context_Create(context_.ptr()))) {
    TPM_LOG(ERROR, result) << "Error connecting to TPM.";
    return false;
  }
  // We retry on failure. It might be that tcsd is starting up.
  for (unsigned int i = 0; i < kTpmConnectRetries; i++) {
    if (TPM_ERROR(result = Tspi_Context_Connect(context_, nullptr))) {
      if (ERROR_CODE(result) == TSS_E_COMM_FAILURE) {
        base::PlatformThread::Sleep(
            base::TimeDelta::FromMilliseconds(kTpmConnectIntervalMs));
      } else {
        TPM_LOG(ERROR, result) << "Error connecting to TPM.";
        return false;
      }
    } else {
      break;
    }
  }
  return (context_.value() != 0);
}

}  // namespace tpm_manager
