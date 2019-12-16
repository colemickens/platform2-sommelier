// Copyright 2020 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_CRYPTO_ERROR_H_
#define CRYPTOHOME_CRYPTO_ERROR_H_

#include <iostream>

namespace cryptohome {

enum class CryptoError {
  CE_NONE = 0,
  CE_TPM_FATAL,
  CE_TPM_COMM_ERROR,
  CE_TPM_DEFEND_LOCK,
  CE_TPM_CRYPTO,
  CE_TPM_REBOOT,
  CE_SCRYPT_CRYPTO,
  CE_OTHER_FATAL,
  CE_OTHER_CRYPTO,
  CE_NO_PUBLIC_KEY_HASH,
  // Low Entropy(LE) credential protection is not supported on this device.
  CE_LE_NOT_SUPPORTED,
  // The LE secret provided during decryption is invalid.
  CE_LE_INVALID_SECRET,
};

// Enum classes are not implicitly converted for log statements.
std::ostream& operator<<(std::ostream& os, const CryptoError& obj);

// Helper function to avoid the double nested if statements involved with
// checking the error pointer. If |error| is |nullptr|, this does nothing.
template <typename ErrorType>
void PopulateError(ErrorType* error, ErrorType error_code) {
  if (error)
    *error = error_code;
}

}  // namespace cryptohome

#endif  // CRYPTOHOME_CRYPTO_ERROR_H_
