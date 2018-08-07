//
// Copyright (C) 2014 The Android Open Source Project
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

#include "trunks/mock_tpm_utility.h"

namespace trunks {

MockTpmUtility::MockTpmUtility() {}
MockTpmUtility::~MockTpmUtility() {}

TPM_RC MockTpmUtility::PinWeaverInsertLeaf(
    uint8_t protocol_version,
    uint64_t label,
    const std::string& h_aux,
    const brillo::SecureBlob& le_secret,
    const brillo::SecureBlob& he_secret,
    const brillo::SecureBlob& reset_secret,
    const std::map<uint32_t, uint32_t>& delay_schedule,
    const ValidPcrCriteria& valid_pcr_criteria,
    uint32_t* result_code,
    std::string* root_hash,
    std::string* cred_metadata,
    std::string* mac) {
  return TPM_RC_SUCCESS;
}

TPM_RC MockTpmUtility::PinWeaverTryAuth(
    uint8_t protocol_version,
    const brillo::SecureBlob& le_secret,
    const std::string& h_aux,
    const std::string& cred_metadata,
    uint32_t* result_code,
    std::string* root_hash,
    uint32_t* seconds_to_wait,
    brillo::SecureBlob* he_secret,
    brillo::SecureBlob* reset_secret,
    std::string* cred_metadata_out,
    std::string* mac_out) {
  return TPM_RC_SUCCESS;
}

}  // namespace trunks
