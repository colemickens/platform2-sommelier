/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef HARDWARE_VERIFIER_HW_VERIFICATION_SPEC_GETTER_IMPL_H_
#define HARDWARE_VERIFIER_HW_VERIFICATION_SPEC_GETTER_IMPL_H_

#include <base/files/file_path.h>

#include "hardware_verifier/hw_verification_spec_getter.h"

namespace hardware_verifier {

// The actual implementation to the |HwVerificationSpecGetter|.
class HwVerificationSpecGetterImpl : public HwVerificationSpecGetter {
 public:
  HwVerificationSpecGetterImpl();

  base::Optional<HwVerificationSpec> GetDefault() const override;
  base::Optional<HwVerificationSpec> GetFromFile(
      const base::FilePath& file_path) const override;

 private:
  friend class HwVerificationSpecGetterImplTest;

  base::FilePath root_;

  // For unittest purpose, whether to check if |cros_debug| flag is 1 or not
  // in |GetFromFile| method.
  bool check_cros_debug_flag_;

  DISALLOW_COPY_AND_ASSIGN(HwVerificationSpecGetterImpl);
};

}  // namespace hardware_verifier

#endif  // HARDWARE_VERIFIER_HW_VERIFICATION_SPEC_GETTER_IMPL_H_
