//
// Copyright (C) 2015 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

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
