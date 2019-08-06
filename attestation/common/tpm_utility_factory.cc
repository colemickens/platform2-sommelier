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

#include "attestation/common/tpm_utility_factory.h"

#if USE_TPM2
#include "attestation/common/tpm_utility_v2.h"
#else
#include "attestation/common/tpm_utility_v1.h"
#endif

namespace attestation {

TpmUtility* TpmUtilityFactory::New() {
#if USE_TPM2
  return new TpmUtilityV2();
#else
  return new TpmUtilityV1();
#endif
}

}  // namespace attestation
