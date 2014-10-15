// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "trunks/tpm_utility_impl.h"

#include <base/logging.h>
#include <base/memory/scoped_ptr.h>

#include "trunks/authorization_delegate.h"
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

TPM_RC TpmUtilityImpl::InitializeTpm() {
  TPM_RC result = TPM_RC_SUCCESS;
  scoped_ptr<TpmState> tpm_state(factory_.GetTpmState());
  result = tpm_state->Initialize();
  if (result) {
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
      return result;
    }
    scoped_ptr<AuthorizationDelegate> authorization(
        factory_.GetPasswordAuthorization(kPlatformPassword));
    result = SetGlobalWriteLock(authorization.get());
    if (result) {
      return result;
    }
    result = DisablePlatformHierarchy(authorization.get());
    if (result) {
      return result;
    }
  }
  return TPM_RC_SUCCESS;
}

TPM_RC TpmUtilityImpl::SetPlatformAuthorization(const std::string& password) {
  NullAuthorizationDelegate null_delegate;
  CHECK_LE(password.size(), 32);
  return factory_.GetTpm()->HierarchyChangeAuthSync(
      TPM_RH_PLATFORM,
      NameFromHandle(TPM_RH_PLATFORM),
      Make_TPM2B_DIGEST(password),
      &null_delegate);
}

TPM_RC TpmUtilityImpl::SetGlobalWriteLock(
    AuthorizationDelegate* authorization) {
  return factory_.GetTpm()->NV_GlobalWriteLockSync(
      TPM_RH_PLATFORM,
      NameFromHandle(TPM_RH_PLATFORM),
      authorization);
}

TPM_RC TpmUtilityImpl::DisablePlatformHierarchy(
    AuthorizationDelegate* authorization) {
  return factory_.GetTpm()->HierarchyControlSync(
      TPM_RH_PLATFORM,  // The authorizing entity.
      NameFromHandle(TPM_RH_PLATFORM),
      TPM_RH_PLATFORM,  // The target hierarchy.
      NameFromHandle(TPM_RH_PLATFORM),
      0,  // Disable.
      authorization);
}

}  // namespace trunks
