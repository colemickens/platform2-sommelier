// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_SIGNATURE_SEALING_BACKEND_TPM1_IMPL_H_
#define CRYPTOHOME_SIGNATURE_SEALING_BACKEND_TPM1_IMPL_H_

#include <stdint.h>
#include <map>
#include <memory>
#include <set>
#include <vector>

#include <base/macros.h>
#include <brillo/secure_blob.h>

#include "cryptohome/signature_sealing_backend.h"

namespace cryptohome {

class TpmImpl;

// Implementation of signature-sealing operations for TPM 1.2. Based on the
// Certified Migratable Key functionality, with the CMK's private key contents
// playing the role of the sealed secret. The CMK is of 2048-bit size.
//
// Only the |kRsassaPkcs1V15Sha1| algorithm is supported by this implementation.
//
// The PCR binding isn't directly supported by this class, and the corresponding
// parameters are ignored. PCR-binding of the secret can be achieved by
// providing the delegate which is itself bound to required PCRs.
class SignatureSealingBackendTpm1Impl final : public SignatureSealingBackend {
 public:
  explicit SignatureSealingBackendTpm1Impl(TpmImpl* tpm);
  ~SignatureSealingBackendTpm1Impl() override;

  // SignatureSealingBackend:
  bool CreateSealedSecret(
      const brillo::Blob& public_key_spki_der,
      const std::vector<ChallengeSignatureAlgorithm>& key_algorithms,
      const std::vector<
          std::map<uint32_t, brillo::Blob>>& /* pcr_restrictions */,
      const brillo::Blob& delegate_blob,
      const brillo::Blob& delegate_secret,
      SignatureSealedData* sealed_secret_data) override;
  std::unique_ptr<UnsealingSession> CreateUnsealingSession(
      const SignatureSealedData& sealed_secret_data,
      const brillo::Blob& public_key_spki_der,
      const std::vector<ChallengeSignatureAlgorithm>& key_algorithms,
      const brillo::Blob& delegate_blob,
      const brillo::Blob& delegate_secret) override;

 private:
  // Unowned.
  TpmImpl* const tpm_;

  DISALLOW_COPY_AND_ASSIGN(SignatureSealingBackendTpm1Impl);
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_SIGNATURE_SEALING_BACKEND_TPM1_IMPL_H_
