// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
