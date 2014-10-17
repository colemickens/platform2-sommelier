// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TRUNKS_TPM_STATE_IMPL_H_
#define TRUNKS_TPM_STATE_IMPL_H_

#include "trunks/tpm_state.h"

#include <base/macros.h>
#include <chromeos/chromeos_export.h>

#include "trunks/tpm_generated.h"

namespace trunks {

class TrunksFactory;

// TpmStateImpl is the default implementation of the TpmState interface.
class CHROMEOS_EXPORT TpmStateImpl : public TpmState {
 public:
  explicit TpmStateImpl(const TrunksFactory& factory);
  virtual ~TpmStateImpl();

  // TpmState methods.
  TPM_RC Initialize() override;
  bool IsOwnerPasswordSet() override;
  bool IsEndorsementPasswordSet() override;
  bool IsLockoutPasswordSet() override;
  bool IsInLockout() override;
  bool IsPlatformHierarchyEnabled() override;
  bool WasShutdownOrderly() override;

 private:
  const TrunksFactory& factory_;
  bool initialized_;
  TPMA_PERMANENT permanent_flags_;
  TPMA_STARTUP_CLEAR startup_clear_flags_;

  DISALLOW_COPY_AND_ASSIGN(TpmStateImpl);
};

}  // namespace trunks

#endif  // TRUNKS_TPM_STATE_IMPL_H_
