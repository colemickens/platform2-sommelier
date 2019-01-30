/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef HARDWARE_VERIFIER_HW_VERIFICATION_SPEC_GETTER_FAKE_H_
#define HARDWARE_VERIFIER_HW_VERIFICATION_SPEC_GETTER_FAKE_H_

#include <map>
#include <string>

#include "hardware_verifier/hardware_verifier.pb.h"
#include "hardware_verifier/hw_verification_spec_getter.h"

namespace hardware_verifier {

// A mock implementation of |VerificationPaylaodGetter| for testing purpose.
class FakeHwVerificationSpecGetter : public HwVerificationSpecGetter {
 public:
  using FileHwVerificationSpecs = std::map<std::string, HwVerificationSpec>;

  FakeHwVerificationSpecGetter();

  base::Optional<HwVerificationSpec> GetDefault() const override;
  base::Optional<HwVerificationSpec> GetFromFile(
      const base::FilePath& file_path) const override;

  void SetDefaultInvalid();
  void set_default(const HwVerificationSpec& vp);
  void set_files(const FileHwVerificationSpecs& fvps);

 private:
  bool is_default_vp_valid_;
  HwVerificationSpec default_vp_;
  FileHwVerificationSpecs file_vps_;
};

}  // namespace hardware_verifier

#endif  // HARDWARE_VERIFIER_HW_VERIFICATION_SPEC_GETTER_FAKE_H_
