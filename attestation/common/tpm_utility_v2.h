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

#ifndef ATTESTATION_COMMON_TPM_UTILITY_V2_H_
#define ATTESTATION_COMMON_TPM_UTILITY_V2_H_

#include "attestation/common/tpm_utility.h"

#include <base/macros.h>

namespace attestation {

// A TpmUtility implementation for TPM v2.0 modules.
class TpmUtilityV2 : public TpmUtility {
 public:
  TpmUtilityV2() = default;
  ~TpmUtilityV2() override;

  // Initializes a TpmUtilityV2 instance. This method must be called
  // successfully before calling any other methods.
  bool Initialize() override;

  // TpmUtility methods.
  bool IsTpmReady() override;
  bool ActivateIdentity(const std::string& delegate_blob,
                        const std::string& delegate_secret,
                        const std::string& identity_key_blob,
                        const std::string& asym_ca_contents,
                        const std::string& sym_ca_attestation,
                        std::string* credential) override;
  bool CreateCertifiedKey(KeyType key_type,
                          KeyUsage key_usage,
                          const std::string& identity_key_blob,
                          const std::string& external_data,
                          std::string* key_blob,
                          std::string* public_key,
                          std::string* public_key_tpm_format,
                          std::string* key_info,
                          std::string* proof) override;
  bool SealToPCR0(const std::string& data, std::string* sealed_data) override;
  bool Unseal(const std::string& sealed_data, std::string* data) override;
  bool GetEndorsementPublicKey(std::string* public_key) override;
  bool Unbind(const std::string& key_blob,
              const std::string& bound_data,
              std::string* data) override;
  bool Sign(const std::string& key_blob,
            const std::string& data_to_sign,
            std::string* signature) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TpmUtilityV2);
};

}  // namespace attestation

#endif  // ATTESTATION_COMMON_TPM_UTILITY_V2_H_
