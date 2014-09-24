// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TRUNKS_TPM_UTILITY_H_
#define TRUNKS_TPM_UTILITY_H_

#include <base/macros.h>
#include <chromeos/chromeos_export.h>

#include "trunks/tpm_generated.h"

namespace trunks {

// An interface which provides common TPM operations.
class CHROMEOS_EXPORT TpmUtility {
 public:
  TpmUtility() {}
  virtual ~TpmUtility() {}

  // Synchronously prepares a TPM for use by Chromium OS. Typically this done by
  // the platform firmware and, in that case, this method has no effect.
  virtual TPM_RC InitializeTpm() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(TpmUtility);
};

}  // namespace trunks

#endif  // TRUNKS_TPM_UTILITY_H_
