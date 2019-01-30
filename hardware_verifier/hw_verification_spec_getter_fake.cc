/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hardware_verifier/hw_verification_spec_getter_fake.h"

namespace hardware_verifier {

FakeHwVerificationSpecGetter::FakeHwVerificationSpecGetter()
    : is_default_vp_valid_(false), default_vp_(), file_vps_() {}

base::Optional<HwVerificationSpec> FakeHwVerificationSpecGetter::GetDefault()
    const {
  if (!is_default_vp_valid_) {
    return base::nullopt;
  }
  return default_vp_;
}

base::Optional<HwVerificationSpec> FakeHwVerificationSpecGetter::GetFromFile(
    const base::FilePath& file_path) const {
  const auto it = file_vps_.find(file_path.value());
  if (it != file_vps_.end()) {
    return it->second;
  }
  return base::nullopt;
}

void FakeHwVerificationSpecGetter::SetDefaultInvalid() {
  is_default_vp_valid_ = false;
}

void FakeHwVerificationSpecGetter::set_default(const HwVerificationSpec& vp) {
  is_default_vp_valid_ = true;
  default_vp_ = vp;
}

void FakeHwVerificationSpecGetter::set_files(
    const FileHwVerificationSpecs& fvps) {
  file_vps_ = fvps;
}

}  // namespace hardware_verifier
