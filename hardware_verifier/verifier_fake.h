/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef HARDWARE_VERIFIER_VERIFIER_FAKE_H_
#define HARDWARE_VERIFIER_VERIFIER_FAKE_H_

#include <runtime_probe/proto_bindings/runtime_probe.pb.h>

#include "hardware_verifier/hardware_verifier.pb.h"
#include "hardware_verifier/verifier.h"

namespace hardware_verifier {

class FakeVerifier : public Verifier {
 public:
  FakeVerifier();

  base::Optional<HwVerificationReport> Verify(
      const runtime_probe::ProbeResult& probe_result,
      const HwVerificationSpec& hw_verification_spec) const override;

  void SetVerifySuccess(const HwVerificationReport& hw_verification_report);

  void SetVerifyFail();

 private:
  bool pass_;
  HwVerificationReport hw_verification_report_;
};

}  // namespace hardware_verifier

#endif  // HARDWARE_VERIFIER_VERIFIER_FAKE_H_
