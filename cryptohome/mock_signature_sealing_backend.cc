// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/mock_signature_sealing_backend.h"

using brillo::SecureBlob;

namespace cryptohome {

MockSignatureSealingBackend::MockSignatureSealingBackend() = default;

MockSignatureSealingBackend::~MockSignatureSealingBackend() = default;

std::unique_ptr<SignatureSealingBackend::UnsealingSession>
MockSignatureSealingBackend::CreateUnsealingSession(
    const SignatureSealedData& sealed_secret_data,
    const SecureBlob& public_key_spki_der,
    const std::vector<Algorithm>& key_algorithms,
    const SecureBlob& delegate_blob,
    const SecureBlob& delegate_secret) {
  return std::unique_ptr<UnsealingSession>(CreateUnsealingSessionImpl(
      sealed_secret_data, public_key_spki_der, key_algorithms, delegate_blob,
      delegate_secret));
}

MockUnsealingSession::MockUnsealingSession() = default;

MockUnsealingSession::~MockUnsealingSession() = default;

}  // namespace cryptohome
