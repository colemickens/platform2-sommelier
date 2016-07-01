//
// Copyright (C) 2016 The Android Open Source Project
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

#include "attestation/common/tpm_utility_v2.h"

#include <base/logging.h>

namespace attestation {

TpmUtilityV2::~TpmUtilityV2() {}

bool TpmUtilityV2::Initialize() {
  LOG(ERROR) << __func__ << ": Not Implemented.";
  return true;
}

bool TpmUtilityV2::IsTpmReady() {
  LOG(ERROR) << __func__ << ": Not Implemented.";
  return false;
}

bool TpmUtilityV2::ActivateIdentity(const std::string& delegate_blob,
                                    const std::string& delegate_secret,
                                    const std::string& identity_key_blob,
                                    const std::string& asym_ca_contents,
                                    const std::string& sym_ca_attestation,
                                    std::string* credential) {
  LOG(ERROR) << __func__ << ": Not Implemented.";
  return false;
}

bool TpmUtilityV2::CreateCertifiedKey(KeyType key_type,
                                      KeyUsage key_usage,
                                      const std::string& identity_key_blob,
                                      const std::string& external_data,
                                      std::string* key_blob,
                                      std::string* public_key,
                                      std::string* public_key_tpm_format,
                                      std::string* key_info,
                                      std::string* proof) {
  LOG(ERROR) << __func__ << ": Not Implemented.";
  return false;
}

bool TpmUtilityV2::SealToPCR0(const std::string& data,
                              std::string* sealed_data) {
  LOG(ERROR) << __func__ << ": Not Implemented.";
  return false;
}

bool TpmUtilityV2::Unseal(const std::string& sealed_data, std::string* data) {
  LOG(ERROR) << __func__ << ": Not Implemented.";
  return false;
}

bool TpmUtilityV2::GetEndorsementPublicKey(std::string* public_key) {
  LOG(ERROR) << __func__ << ": Not Implemented.";
  return false;
}

bool TpmUtilityV2::Unbind(const std::string& key_blob,
                          const std::string& bound_data,
                          std::string* data) {
  LOG(ERROR) << __func__ << ": Not Implemented.";
  return false;
}

bool TpmUtilityV2::Sign(const std::string& key_blob,
                        const std::string& data_to_sign,
                        std::string* signature) {
  LOG(ERROR) << __func__ << ": Not Implemented.";
  return false;
}

}  // namespace attestation
