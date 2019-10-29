// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TRUNKS_TPM_CONSTANTS_H_
#define TRUNKS_TPM_CONSTANTS_H_

#include "trunks/tpm_generated.h"

namespace trunks {

// TPM Object Attributes.
constexpr TPMA_OBJECT kFixedTPM = 1U << 1;
constexpr TPMA_OBJECT kFixedParent = 1U << 4;
constexpr TPMA_OBJECT kSensitiveDataOrigin = 1U << 5;
constexpr TPMA_OBJECT kUserWithAuth = 1U << 6;
constexpr TPMA_OBJECT kAdminWithPolicy = 1U << 7;
constexpr TPMA_OBJECT kNoDA = 1U << 10;
constexpr TPMA_OBJECT kRestricted = 1U << 16;
constexpr TPMA_OBJECT kDecrypt = 1U << 17;
constexpr TPMA_OBJECT kSign = 1U << 18;

// TPM NV Index Attributes, defined in TPM Spec Part 2 section 13.2.
constexpr TPMA_NV TPMA_NV_PPWRITE = 1U << 0;
constexpr TPMA_NV TPMA_NV_OWNERWRITE = 1U << 1;
constexpr TPMA_NV TPMA_NV_AUTHWRITE = 1U << 2;
constexpr TPMA_NV TPMA_NV_POLICYWRITE = 1U << 3;
constexpr TPMA_NV TPMA_NV_COUNTER = 1U << 4;
constexpr TPMA_NV TPMA_NV_BITS = 1U << 5;
constexpr TPMA_NV TPMA_NV_EXTEND = 1U << 6;
constexpr TPMA_NV TPMA_NV_POLICY_DELETE = 1U << 10;
constexpr TPMA_NV TPMA_NV_WRITELOCKED = 1U << 11;
constexpr TPMA_NV TPMA_NV_WRITEALL = 1U << 12;
constexpr TPMA_NV TPMA_NV_WRITEDEFINE = 1U << 13;
constexpr TPMA_NV TPMA_NV_WRITE_STCLEAR = 1U << 14;
constexpr TPMA_NV TPMA_NV_GLOBALLOCK = 1U << 15;
constexpr TPMA_NV TPMA_NV_PPREAD = 1U << 16;
constexpr TPMA_NV TPMA_NV_OWNERREAD = 1U << 17;
constexpr TPMA_NV TPMA_NV_AUTHREAD = 1U << 18;
constexpr TPMA_NV TPMA_NV_POLICYREAD = 1U << 19;
constexpr TPMA_NV TPMA_NV_NO_DA = 1U << 25;
constexpr TPMA_NV TPMA_NV_ORDERLY = 1U << 26;
constexpr TPMA_NV TPMA_NV_CLEAR_STCLEAR = 1U << 27;
constexpr TPMA_NV TPMA_NV_READLOCKED = 1U << 28;
constexpr TPMA_NV TPMA_NV_WRITTEN = 1U << 29;
constexpr TPMA_NV TPMA_NV_PLATFORMCREATE = 1U << 30;
constexpr TPMA_NV TPMA_NV_READ_STCLEAR = 1U << 31;

// TPM Vendor-Specific commands (TPM Spec Part 2, section 6.5.1)
constexpr TPM_CC TPM_CC_VENDOR_SPECIFIC_MASK = 1U << 29;

// This needs to be used to be backwards compatible with older Cr50 versions.
constexpr TPM_CC TPM_CC_CR50_EXTENSION_COMMAND = 0xbaccd00a;
}  // namespace trunks

#endif  // TRUNKS_TPM_CONSTANTS_H_
