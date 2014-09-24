// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TRUNKS_TPM_STATE_H_
#define TRUNKS_TPM_STATE_H_

#include <base/macros.h>
#include <chromeos/chromeos_export.h>

#include "trunks/tpm_generated.h"

namespace trunks {

// TpmState is an interface which provides access to TPM state information.
class CHROMEOS_EXPORT TpmState {
 public:
  TpmState() {}
  virtual ~TpmState() {}

  // Initializes based on the current TPM state. This method must be called once
  // before any other method. It may be called multiple times to refresh the
  // state information.
  virtual TPM_RC Initialize() = 0;

  // Returns true iff TPMA_PERMANENT:inLockout is set.
  virtual bool IsInLockout() = 0;

  // Returns true iff TPMA_STARTUP_CLEAR:phEnable is set.
  virtual bool IsPlatformHierarchyEnabled() = 0;

  // Returns true iff TPMA_STARTUP_CLEAR:orderly is set.
  virtual bool WasShutdownOrderly() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(TpmState);
};

}  // namespace trunks

#endif  // TRUNKS_TPM_STATE_H_
