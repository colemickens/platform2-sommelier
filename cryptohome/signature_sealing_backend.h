// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_SIGNATURE_SEALING_BACKEND_H_
#define CRYPTOHOME_SIGNATURE_SEALING_BACKEND_H_

#include <stdint.h>
#include <map>
#include <memory>
#include <set>
#include <vector>

#include <brillo/secure_blob.h>

#include "signature_sealed_data.pb.h"  // NOLINT(build/include)

namespace cryptohome {

// Interface for performing signature-sealing operations using the TPM.
// Implementations of this interface are exposed by the Tpm class subclasses.
class SignatureSealingBackend {
 public:
  // A cryptographic signature algorithm to be used for sealing and unsealing.
  enum class Algorithm {
    kRsassaPkcs1V15Sha1,
    kRsassaPkcs1V15Sha256,
    kRsassaPkcs1V15Sha384,
    kRsassaPkcs1V15Sha512,
  };

  // Interface for a session of unsealing the sealed secret.
  // Instances can be obtained via
  // SignatureSealingBackend::CreateUnsealingSession().
  //
  // Unless the implementation documents otherwise, all methods of this class
  // have to be called from a single thread - the thread on which
  // SignatureSealingBackend::CreateUnsealingSession() was called.
  class UnsealingSession {
   public:
    using Algorithm = SignatureSealingBackend::Algorithm;

    virtual ~UnsealingSession() = default;

    // Returns the algorithm to be used for signing the challenge value.
    virtual Algorithm GetChallengeAlgorithm() = 0;

    // Returns the challenge value to be signed.
    virtual brillo::Blob GetChallengeValue() = 0;

    // Attempts to complete the unsealing, given the signature of the challenge
    // value.
    //
    // Should normally be called only once.
    //
    // Parameters
    //   signed_challenge_value - Signature of the blob returned by
    //                            GetChallengeValue() using the algorithm as
    //                            returned by GetChallengeAlgorithm().
    //   unsealed_value - The unsealed value, if the function returned true.
    virtual bool Unseal(const brillo::Blob& signed_challenge_value,
                        brillo::SecureBlob* unsealed_value) = 0;
  };

  virtual ~SignatureSealingBackend() = default;

  // Creates a random secret and seals it with the specified key, so that
  // unsealing is gated on providing a valid signature for the challenge.
  //
  // Parameters
  //   public_key_spki_der - The DER-encoded Subject Public Key Info of the key
  //                         using which the secret should be sealed.
  //   key_algorithms - The list of signature algorithms supported by the key.
  //                    Listed in the order of preference (starting from the
  //                    most preferred); however, the implementation is
  //                    permitted to ignore this order.
  //   pcr_values - Mapping from PCR indexes to their values, to which the
  //                created secret should be bound.
  //   delegate_blob - The blob for the owner delegation.
  //   delegate_secret - The delegate secret for the delegate blob.
  //   sealed_secret_data - The created sealed secret value.
  virtual bool CreateSealedSecret(
      const brillo::Blob& public_key_spki_der,
      const std::vector<Algorithm>& key_algorithms,
      const std::map<uint32_t, brillo::Blob>& pcr_values,
      const brillo::Blob& delegate_blob,
      const brillo::Blob& delegate_secret,
      SignatureSealedData* sealed_secret_data) = 0;

  // Initiates a session for unsealing the passed sealed data.
  // Note: the implementation may impose restrictions on the number of unsealing
  // sessions that are allowed to coexist simultaneously.
  //
  // Parameters
  //   sealed_secret_data - The sealed value, as created by
  //                        CreateSealedSecret().
  //   public_key_spki_der - The DER-encoded Subject Public Key Info of the key
  //                         to be challenged for unsealing.
  //   key_algorithms - The list of signature algorithms supported by the key.
  //                    Listed in the order of preference (starting from the
  //                    most preferred); however, the implementation is
  //                    permitted to ignore this order.
  //   delegate_blob - The blob for the owner delegation.
  //   delegate_secret - The delegate secret for the delegate blob.
  virtual std::unique_ptr<UnsealingSession> CreateUnsealingSession(
      const SignatureSealedData& sealed_secret_data,
      const brillo::Blob& public_key_spki_der,
      const std::vector<Algorithm>& key_algorithms,
      const brillo::Blob& delegate_blob,
      const brillo::Blob& delegate_secret) = 0;
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_SIGNATURE_SEALING_BACKEND_H_
