/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef HARDWARE_VERIFIER_VERIFIER_IMPL_H_
#define HARDWARE_VERIFIER_VERIFIER_IMPL_H_

#include "hardware_verifier/verifier.h"

namespace hardware_verifier {

class VerifierImpl : public Verifier {
 public:
  base::Optional<HwVerificationReport> Verify(
      const runtime_probe::ProbeResult& probe_result,
      const HwVerificationSpec& hw_verification_spec) const override;
};

}  // namespace hardware_verifier

#endif  // HARDWARE_VERIFIER_VERIFIER_IMPL_H_
