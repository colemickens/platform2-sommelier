// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TPM_MANAGER_SERVER_TPM_UTIL_H_
#define TPM_MANAGER_SERVER_TPM_UTIL_H_

#include <string>

#include <trousers/tss.h>

namespace tpm_manager {

#define TPM_LOG(severity, result)                               \
  LOG(severity) << "TPM error 0x" << std::hex << result << " (" \
                << Trspi_Error_String(result) << "): "

// Don't use directly, use GetDefaultOwnerPassword().
const char kDefaultOwnerPassword[] = TSS_WELL_KNOWN_SECRET;
// Owner password is human-readable, so produce N random bytes and then
// hexdump them into N*2 password bytes. For other passwords, just generate
// N*2 random bytes.
const size_t kOwnerPasswordRandomBytes = 10;
const size_t kDefaultPasswordSize = kOwnerPasswordRandomBytes * 2;

// Builds the default owner password used before TPM is fully initialized.
//
// NOTE: This method should be used by TPM 1.2 only.
inline std::string GetDefaultOwnerPassword() {
  return std::string(kDefaultOwnerPassword, kDefaultPasswordSize);
}

}  // namespace tpm_manager

#endif  // TPM_MANAGER_SERVER_TPM_UTIL_H_
