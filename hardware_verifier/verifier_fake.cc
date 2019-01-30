/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hardware_verifier/verifier_fake.h"

#include <base/optional.h>

namespace hardware_verifier {

FakeVerifier::FakeVerifier() : pass_(false), hw_verification_report_() {}

base::Optional<HwVerificationReport> FakeVerifier::Verify(
    const runtime_probe::ProbeResult& probe_result,
    const HwVerificationSpec& hw_verification_spec) const {
  if (pass_) {
    return hw_verification_report_;
  }
  return base::nullopt;
}

void FakeVerifier::SetVerifySuccess(
    const HwVerificationReport& hw_verification_report) {
  pass_ = true;
  hw_verification_report_ = hw_verification_report;
}

void FakeVerifier::SetVerifyFail() {
  pass_ = false;
}

}  // namespace hardware_verifier
