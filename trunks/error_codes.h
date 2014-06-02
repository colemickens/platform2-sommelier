// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TRUNKS_ERROR_CODES_H_
#define TRUNKS_ERROR_CODES_H_

#include "trunks/tpm_generated.h"  // For TPM_RC.

namespace trunks {

// Use the TPM_RC type but with different layer bits (12 - 15). Choose the layer
// value arbitrarily. Currently TSS2 uses 9 for TCTI and 8 for SAPI.
const TPM_RC kTrunksErrorBase = (7 << 12);
const TPM_RC TRUNKS_RC_AUTHORIZATION_FAILED = kTrunksErrorBase + 1;
const TPM_RC TRUNKS_RC_ENCRYPTION_FAILED = kTrunksErrorBase + 2;

}  // namespace trunks

#endif  // TRUNKS_ERROR_CODES_H_
