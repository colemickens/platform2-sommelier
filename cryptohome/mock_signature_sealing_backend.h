// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_MOCK_SIGNATURE_SEALING_BACKEND_H_
#define CRYPTOHOME_MOCK_SIGNATURE_SEALING_BACKEND_H_

#include <cstdint>
#include <map>
#include <memory>
#include <vector>

#include <brillo/secure_blob.h>
#include <gmock/gmock.h>

#include "cryptohome/signature_sealing_backend.h"

namespace cryptohome {

class SignatureSealedData;

class MockSignatureSealingBackend : public SignatureSealingBackend {
 public:
  MockSignatureSealingBackend();
  ~MockSignatureSealingBackend() override;

  MOCK_METHOD7(CreateSealedSecret,
               bool(const brillo::Blob&,
                    const std::vector<ChallengeSignatureAlgorithm>&,
                    const std::vector<std::map<uint32_t, brillo::Blob>>&,
                    const brillo::Blob&,
                    const brillo::Blob&,
                    brillo::SecureBlob*,
                    SignatureSealedData*));

  // Wrap a mockable method to workaround gmock's issue with noncopyable types.
  std::unique_ptr<UnsealingSession> CreateUnsealingSession(
      const SignatureSealedData& sealed_secret_data,
      const brillo::Blob& public_key_spki_der,
      const std::vector<ChallengeSignatureAlgorithm>& key_algorithms,
      const brillo::Blob& delegate_blob,
      const brillo::Blob& delegate_secret) override;
  // Equivalent of CreateUnsealingSession(), but returns result via a raw owned
  // pointer.
  MOCK_METHOD5(
      CreateUnsealingSessionImpl,
      UnsealingSession*(const SignatureSealedData&,
                        const brillo::Blob&,
                        const std::vector<ChallengeSignatureAlgorithm>&,
                        const brillo::Blob&,
                        const brillo::Blob&));
};

class MockUnsealingSession : public SignatureSealingBackend::UnsealingSession {
 public:
  MockUnsealingSession();
  ~MockUnsealingSession() override;

  MOCK_METHOD0(GetChallengeAlgorithm, ChallengeSignatureAlgorithm());
  MOCK_METHOD0(GetChallengeValue, brillo::Blob());
  MOCK_METHOD2(Unseal, bool(const brillo::Blob&, brillo::SecureBlob*));
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_MOCK_SIGNATURE_SEALING_BACKEND_H_
