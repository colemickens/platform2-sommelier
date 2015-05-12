// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TRUNKS_TPM_CONSTANTS_H_
#define TRUNKS_TPM_CONSTANTS_H_

#include "trunks/tpm_generated.h"

namespace trunks {

// TPM Object Attributes.
const TPMA_OBJECT kFixedTPM = 1U << 1;
const TPMA_OBJECT kFixedParent = 1U << 4;
const TPMA_OBJECT kSensitiveDataOrigin = 1U << 5;
const TPMA_OBJECT kUserWithAuth = 1U << 6;
const TPMA_OBJECT kAdminWithPolicy = 1U << 7;
const TPMA_OBJECT kNoDA = 1U << 10;
const TPMA_OBJECT kRestricted = 1U << 16;
const TPMA_OBJECT kDecrypt = 1U << 17;
const TPMA_OBJECT kSign = 1U << 18;

// TPM NV Index Attributes, defined in TPM Spec Part 2 section 13.2.
const TPMA_NV TPMA_NV_OWNERWRITE = 1U << 1;
const TPMA_NV TPMA_NV_WRITELOCKED = 1U << 11;
const TPMA_NV TPMA_NV_WRITEDEFINE = 1U << 13;
const TPMA_NV TPMA_NV_AUTHREAD = 1U << 18;
const TPMA_NV TPMA_NV_WRITTEN = 1U << 29;

}  // namespace trunks

#endif  // TRUNKS_TPM_CONSTANTS_H_
