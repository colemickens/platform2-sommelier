// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TRUNKS_TPM_STATE_IMPL_H_
#define TRUNKS_TPM_STATE_IMPL_H_

#include "trunks/tpm_state.h"

#include <base/macros.h>

#include "trunks/tpm_generated.h"
#include "trunks/trunks_export.h"

namespace trunks {

class TrunksFactory;

// TpmStateImpl is the default implementation of the TpmState interface.
class TRUNKS_EXPORT TpmStateImpl : public TpmState {
 public:
  explicit TpmStateImpl(const TrunksFactory& factory);
  ~TpmStateImpl() override;

  // TpmState methods.
  TPM_RC Initialize() override;
  bool IsOwnerPasswordSet() override;
  bool IsEndorsementPasswordSet() override;
  bool IsLockoutPasswordSet() override;
  bool IsOwned() override;
  bool IsInLockout() override;
  bool IsPlatformHierarchyEnabled() override;
  bool IsStorageHierarchyEnabled() override;
  bool IsEndorsementHierarchyEnabled() override;
  bool IsEnabled() override;
  bool WasShutdownOrderly() override;
  bool IsRSASupported() override;
  bool IsECCSupported() override;
  uint32_t GetLockoutCounter() override;
  uint32_t GetLockoutThreshold() override;
  uint32_t GetLockoutInterval() override;
  uint32_t GetLockoutRecovery() override;

 private:
  // This helped method calls Tpm2_GetCapability with TPM_CAP_TPM_PROPERTIES
  // and |property|. The returned structure is validated, and the value returned
  // is stored in the out argument |value|. Returns TPM_RC_SUCCESS on success.
  TPM_RC GetTpmProperty(uint32_t property, uint32_t* value);

  const TrunksFactory& factory_;
  bool initialized_;
  TPMA_PERMANENT permanent_flags_;
  TPMA_STARTUP_CLEAR startup_clear_flags_;
  uint32_t lockout_counter_;
  uint32_t lockout_threshold_;
  uint32_t lockout_interval_;
  uint32_t lockout_recovery_;
  TPMA_ALGORITHM rsa_flags_;
  TPMA_ALGORITHM ecc_flags_;

  DISALLOW_COPY_AND_ASSIGN(TpmStateImpl);
};

}  // namespace trunks

#endif  // TRUNKS_TPM_STATE_IMPL_H_
