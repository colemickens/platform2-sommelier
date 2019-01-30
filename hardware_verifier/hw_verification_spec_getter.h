/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef HARDWARE_VERIFIER_HW_VERIFICATION_SPEC_GETTER_H_
#define HARDWARE_VERIFIER_HW_VERIFICATION_SPEC_GETTER_H_

#include <base/files/file_path.h>
#include <base/optional.h>

#include "hardware_verifier/hardware_verifier.pb.h"

namespace hardware_verifier {

// Interface that provides ways to retrieve |VerificationPaylaod| messages.
class HwVerificationSpecGetter {
 public:
  virtual ~HwVerificationSpecGetter() = default;

  // Reads the |HwVerificationSpec| message from the default path in rootfs.
  //
  // The default path is |/etc/hardware_verifier/verfiication_payload.prototxt|.
  //
  // @return A |HwVerificationSpec| message if it succeeds.
  virtual base::Optional<HwVerificationSpec> GetDefault() const = 0;

  // Reads the |HwVerificationSpec| message from the given path.
  //
  // The given file name must ends with ".prototxt" and the content must be
  // in protobuf text format.  |hardware_verifier| is not allowed to open
  // arbitrary hardware verification spec file if |cros_debug| is 0 so
  // this method does the check and returns |false| in that case.
  //
  // @param file_path: Path to the file that contains the data.
  //
  // @return A |HwVerificationSpec| message if it succeeds.
  virtual base::Optional<HwVerificationSpec> GetFromFile(
      const base::FilePath& file_path) const = 0;
};

}  // namespace hardware_verifier

#endif  // HARDWARE_VERIFIER_HW_VERIFICATION_SPEC_GETTER_H_
