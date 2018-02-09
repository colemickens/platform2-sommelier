// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_SIGNATURE_SEALING_BACKEND_TPM2_IMPL_H_
#define CRYPTOHOME_SIGNATURE_SEALING_BACKEND_TPM2_IMPL_H_

#include <map>
#include <memory>
#include <set>
#include <vector>

#include <base/macros.h>

#include "cryptohome/signature_sealing_backend.h"

namespace cryptohome {

class Tpm2Impl;

// Implementation of signature-sealing operations for TPM 2.0. Based on the
// TPM2_PolicySigned functionality.
class SignatureSealingBackendTpm2Impl final : public SignatureSealingBackend {
 public:
  explicit SignatureSealingBackendTpm2Impl(Tpm2Impl* tpm);
  ~SignatureSealingBackendTpm2Impl() override;

  // SignatureSealingBackend:
  bool CreateSealedSecret(const brillo::SecureBlob& public_key_spki_der,
                          const std::vector<Algorithm>& key_algorithms,
                          const std::map<int, brillo::SecureBlob>& pcr_values,
                          const brillo::SecureBlob& /* delegate_blob */,
                          const brillo::SecureBlob& /* delegate_secret */,
                          SignatureSealedData* sealed_secret_data) override;
  std::unique_ptr<UnsealingSession> CreateUnsealingSession(
      const SignatureSealedData& sealed_secret_data,
      const brillo::SecureBlob& public_key_spki_der,
      const std::vector<Algorithm>& key_algorithms,
      const std::set<int>& bound_pcrs,
      const brillo::SecureBlob& /* delegate_blob */,
      const brillo::SecureBlob& /* delegate_secret */) override;

 private:
  // Unowned.
  Tpm2Impl* const tpm_;

  DISALLOW_COPY_AND_ASSIGN(SignatureSealingBackendTpm2Impl);
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_SIGNATURE_SEALING_BACKEND_TPM2_IMPL_H_
