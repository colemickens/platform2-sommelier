// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBTPMCRYPTO_TPM_H_
#define LIBTPMCRYPTO_TPM_H_

#include <stddef.h>

#include <base/compiler_specific.h>
#include <base/macros.h>
#include <brillo/secure_blob.h>

namespace tpmcrypto {

class Tpm {
 public:
  virtual ~Tpm() = default;

  // Seals a secret to PCR0 with the SRK.
  //
  // Parameters
  //   value - The value to be sealed.
  //   sealed_value - The sealed value.
  //
  // Returns true on success.
  virtual bool SealToPCR0(const brillo::SecureBlob& value,
                          brillo::Blob* sealed_value) WARN_UNUSED_RESULT = 0;

  // Unseals a secret previously sealed with the SRK.
  //
  // Parameters
  //   sealed_value - The sealed value.
  //   value - The original value.
  //
  // Returns true on success.
  virtual bool Unseal(const brillo::Blob& sealed_value,
                      brillo::SecureBlob* value) WARN_UNUSED_RESULT = 0;

 protected:
  Tpm() = default;

  DISALLOW_COPY_AND_ASSIGN(Tpm);
};

}  // namespace tpmcrypto

#endif  // LIBTPMCRYPTO_TPM_H_
