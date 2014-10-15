// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "trunks/tpm_utility_impl.h"

#include <base/logging.h>
#include <base/memory/scoped_ptr.h>
#include <base/stl_util.h>
#include <crypto/secure_hash.h>
#include <crypto/sha2.h>

#include "trunks/authorization_delegate.h"
#include "trunks/error_codes.h"
#include "trunks/null_authorization_delegate.h"
#include "trunks/tpm_state.h"
#include "trunks/trunks_factory.h"

namespace {

const char kPlatformPassword[] = "cros-platform";

// Returns a serialized representation of the unmodified handle. This is useful
// for predefined handle values, like TPM_RH_OWNER. For details on what types of
// handles use this name formula see Table 3 in the TPM 2.0 Library Spec Part 1
// (Section 16 - Names).
std::string NameFromHandle(trunks::TPM_HANDLE handle) {
  std::string name;
  trunks::Serialize_TPM_HANDLE(handle, &name);
  return name;
}

}  // namespace

namespace trunks {

TpmUtilityImpl::TpmUtilityImpl(const TrunksFactory& factory)
    : factory_(factory) {
}

TpmUtilityImpl::~TpmUtilityImpl() {
}

TPM_RC TpmUtilityImpl::Startup() {
  TPM_RC result = TPM_RC_SUCCESS;
  Tpm* tpm = factory_.GetTpm();
  NullAuthorizationDelegate authorization;
  result = tpm->StartupSync(TPM_SU_CLEAR, &authorization);
  // Ignore TPM_RC_INITIALIZE, that means it was already started.
  if (result && result != TPM_RC_INITIALIZE) {
    LOG(ERROR) << __func__ << ": " << GetErrorString(result);
    return result;
  }
  result = tpm->SelfTestSync(YES /* Full test. */, &authorization);
  if (result) {
    LOG(ERROR) << __func__ << ": " << GetErrorString(result);
    return result;
  }
  return TPM_RC_SUCCESS;
}

TPM_RC TpmUtilityImpl::InitializeTpm() {
  TPM_RC result = TPM_RC_SUCCESS;
  scoped_ptr<TpmState> tpm_state(factory_.GetTpmState());
  result = tpm_state->Initialize();
  if (result) {
    LOG(ERROR) << __func__ << ": " << GetErrorString(result);
    return result;
  }
  // Warn about various unexpected conditions.
  if (!tpm_state->WasShutdownOrderly()) {
    LOG(WARNING) << "WARNING: The last TPM shutdown was not orderly.";
  }
  if (tpm_state->IsInLockout()) {
    LOG(WARNING) << "WARNING: The TPM is currently in lockout.";
  }

  // We expect the firmware has already locked down the platform hierarchy. If
  // it hasn't, do it now.
  if (tpm_state->IsPlatformHierarchyEnabled()) {
    result = SetPlatformAuthorization(kPlatformPassword);
    if (result) {
      LOG(ERROR) << __func__ << ": " << GetErrorString(result);
      return result;
    }
    scoped_ptr<AuthorizationDelegate> authorization(
        factory_.GetPasswordAuthorization(kPlatformPassword));
    result = DisablePlatformHierarchy(authorization.get());
    if (result) {
      LOG(ERROR) << __func__ << ": " << GetErrorString(result);
      return result;
    }
  }
  return TPM_RC_SUCCESS;
}

TPM_RC TpmUtilityImpl::StirRandom(const std::string& entropy_data) {
  NullAuthorizationDelegate null_delegate;
  std::string digest = crypto::SHA256HashString(entropy_data);
  TPM2B_SENSITIVE_DATA random_bytes = Make_TPM2B_SENSITIVE_DATA(digest);
  return factory_.GetTpm()->StirRandomSync(random_bytes, &null_delegate);
}

TPM_RC TpmUtilityImpl::GenerateRandom(int num_bytes,
                                      std::string* random_data) {
  NullAuthorizationDelegate null_delegate;
  int bytes_left = num_bytes;
  random_data->clear();
  TPM_RC rc;
  TPM2B_DIGEST digest;
  while (bytes_left > 0) {
    rc = factory_.GetTpm()->GetRandomSync(bytes_left,
                                          &digest,
                                          &null_delegate);
    if (rc) {
      LOG(ERROR) << "Error getting random data from tpm.";
      return rc;
    }
    random_data->append(StringFrom_TPM2B_DIGEST(digest));
    bytes_left -= digest.size;
  }
  CHECK_EQ(random_data->size(), num_bytes);
  return TPM_RC_SUCCESS;
}

TPM_RC TpmUtilityImpl::SetPlatformAuthorization(const std::string& password) {
  scoped_ptr<AuthorizationDelegate> authorization(
      factory_.GetPasswordAuthorization(""));
  CHECK_LE(password.size(), 32);
  TPM_RC result = factory_.GetTpm()->HierarchyChangeAuthSync(
      TPM_RH_PLATFORM,
      NameFromHandle(TPM_RH_PLATFORM),
      Make_TPM2B_DIGEST(password),
      authorization.get());
  if (GetFormatOneError(result) == TPM_RC_BAD_AUTH) {
    // Most likely the platform hierarchy password has already been set.
    return TPM_RC_SUCCESS;
  }
  return result;
}

TPM_RC TpmUtilityImpl::DisablePlatformHierarchy(
    AuthorizationDelegate* authorization) {
  return factory_.GetTpm()->HierarchyControlSync(
      TPM_RH_PLATFORM,  // The authorizing entity.
      NameFromHandle(TPM_RH_PLATFORM),
      TPM_RH_PLATFORM,  // The target hierarchy.
      0,  // Disable.
      authorization);
}

}  // namespace trunks
