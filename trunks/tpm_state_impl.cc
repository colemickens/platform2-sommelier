// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "trunks/tpm_state_impl.h"

#include <base/logging.h>

#include "trunks/error_codes.h"
#include "trunks/tpm_generated.h"
#include "trunks/trunks_factory.h"

namespace {

// From definition of TPMA_PERMANENT.
const trunks::TPMA_PERMANENT kOwnerAuthSetMask = 1U;
const trunks::TPMA_PERMANENT kEndorsementAuthSetMask = 1U << 1;
const trunks::TPMA_PERMANENT kLockoutAuthSetMask = 1U << 2;
const trunks::TPMA_PERMANENT kInLockoutMask = 1U << 9;

// From definition of TPMA_STARTUP_CLEAR.
const trunks::TPMA_STARTUP_CLEAR kPlatformHierarchyMask = 1U;
const trunks::TPMA_STARTUP_CLEAR kOrderlyShutdownMask = 1U << 31;

// From definition of TPMA_ALGORITHM
const trunks::TPMA_ALGORITHM kAsymmetricAlgMask = 1U;

}  // namespace

namespace trunks {

TpmStateImpl::TpmStateImpl(const TrunksFactory& factory)
    : factory_(factory),
      initialized_(false),
      permanent_flags_(0),
      startup_clear_flags_(0),
      rsa_flags_(0),
      ecc_flags_(0) {
}

TpmStateImpl::~TpmStateImpl() {
}

TPM_RC TpmStateImpl::Initialize() {
  Tpm* tpm = factory_.GetTpm();
  TPMI_YES_NO more_data;
  TPMS_CAPABILITY_DATA capability_data;
  TPM_RC result = tpm->GetCapabilitySync(TPM_CAP_TPM_PROPERTIES,
                                         TPM_PT_PERMANENT,
                                         1,  // There is only one value.
                                         &more_data,
                                         &capability_data,
                                         nullptr);
  if (result) {
    LOG(ERROR) << __func__ << ": " << GetErrorString(result);
    return result;
  }
  if (capability_data.capability != TPM_CAP_TPM_PROPERTIES ||
      capability_data.data.tpm_properties.count != 1 ||
      capability_data.data.tpm_properties.tpm_property[0].property !=
            TPM_PT_PERMANENT) {
    LOG(ERROR) << __func__ << ": Unexpected capability data.";
    return SAPI_RC_MALFORMED_RESPONSE;
  }
  permanent_flags_ = capability_data.data.tpm_properties.tpm_property[0].value;

  result = tpm->GetCapabilitySync(TPM_CAP_TPM_PROPERTIES,
                                  TPM_PT_STARTUP_CLEAR,
                                  1,  // There is only one value.
                                  &more_data,
                                  &capability_data,
                                  nullptr);
  if (result) {
    LOG(ERROR) << __func__ << ": " << GetErrorString(result);
    return result;
  }
  if (capability_data.capability != TPM_CAP_TPM_PROPERTIES ||
      capability_data.data.tpm_properties.count != 1 ||
      capability_data.data.tpm_properties.tpm_property[0].property !=
            TPM_PT_STARTUP_CLEAR) {
    LOG(ERROR) << __func__ << ": Unexpected capability data.";
    return SAPI_RC_MALFORMED_RESPONSE;
  }
  startup_clear_flags_ =
      capability_data.data.tpm_properties.tpm_property[0].value;

  result = tpm->GetCapabilitySync(TPM_CAP_ALGS,
                                  TPM_ALG_RSA,
                                  1,  // There is only one value.
                                  &more_data,
                                  &capability_data,
                                  nullptr);
  if (result) {
    LOG(ERROR) << __func__ << ": " << GetErrorString(result);
    return result;
  }
  if (capability_data.capability != TPM_CAP_ALGS ||
      capability_data.data.algorithms.count != 1) {
    LOG(ERROR) << __func__ << ": Unexpected capability data.";
    return SAPI_RC_MALFORMED_RESPONSE;
  }
  if (capability_data.data.algorithms.alg_properties[0].alg == TPM_ALG_RSA) {
    rsa_flags_ =
        capability_data.data.algorithms.alg_properties[0].alg_properties;
  }

  result = tpm->GetCapabilitySync(TPM_CAP_ALGS,
                                  TPM_ALG_ECC,
                                  1,  // There is only one value.
                                  &more_data,
                                  &capability_data,
                                  nullptr);
  if (result) {
    LOG(ERROR) << __func__ << ": " << GetErrorString(result);
    return result;
  }
  if (capability_data.capability != TPM_CAP_ALGS ||
      capability_data.data.algorithms.count != 1) {
    LOG(ERROR) << __func__ << ": Unexpected capability data.";
    return SAPI_RC_MALFORMED_RESPONSE;
  }
  if (capability_data.data.algorithms.alg_properties[0].alg == TPM_ALG_ECC) {
    ecc_flags_ =
        capability_data.data.algorithms.alg_properties[0].alg_properties;
  }
  initialized_ = true;
  return TPM_RC_SUCCESS;
}

bool TpmStateImpl::IsOwnerPasswordSet() {
  CHECK(initialized_);
  return ((permanent_flags_ & kOwnerAuthSetMask) == kOwnerAuthSetMask);
}

bool TpmStateImpl::IsEndorsementPasswordSet() {
  CHECK(initialized_);
  return ((permanent_flags_ & kEndorsementAuthSetMask) ==
          kEndorsementAuthSetMask);
}

bool TpmStateImpl::IsLockoutPasswordSet() {
  CHECK(initialized_);
  return ((permanent_flags_ & kLockoutAuthSetMask) == kLockoutAuthSetMask);
}

bool TpmStateImpl::IsInLockout() {
  CHECK(initialized_);
  return ((permanent_flags_ & kInLockoutMask) == kInLockoutMask);
}

bool TpmStateImpl::IsPlatformHierarchyEnabled() {
  CHECK(initialized_);
  return ((startup_clear_flags_ & kPlatformHierarchyMask) ==
      kPlatformHierarchyMask);
}

bool TpmStateImpl::WasShutdownOrderly() {
  CHECK(initialized_);
  return ((startup_clear_flags_ & kOrderlyShutdownMask) ==
      kOrderlyShutdownMask);
}

bool TpmStateImpl::IsRSASupported() {
  CHECK(initialized_);
  return ((rsa_flags_ & kAsymmetricAlgMask) == kAsymmetricAlgMask);
}

bool TpmStateImpl::IsECCSupported() {
  CHECK(initialized_);
  return ((ecc_flags_ & kAsymmetricAlgMask) == kAsymmetricAlgMask);
}

}  // namespace trunks
