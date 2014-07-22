// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EASY_UNLOCK_FAKE_EASY_UNLOCK_SERVICE_H_
#define EASY_UNLOCK_FAKE_EASY_UNLOCK_SERVICE_H_

#include "easy-unlock/easy_unlock_service.h"

namespace easy_unlock {

// EasyUnlock service to be used in unittests.
class FakeService : public Service {
 public:
  FakeService();
  virtual ~FakeService();

  // easy_unlock::Service overrides:
  virtual void GenerateEcP256KeyPair(std::vector<uint8>* private_key,
                                     std::vector<uint8>* public_key) OVERRIDE;
  virtual std::vector<uint8> PerformECDHKeyAgreement(
      const std::vector<uint8>& private_key,
      const std::vector<uint8>& public_key) OVERRIDE;
  virtual std::vector<uint8> CreateSecureMessage(
      const std::vector<uint8>& payload,
      const std::vector<uint8>& key,
      const std::vector<uint8>& associated_data,
      const std::vector<uint8>& public_metadata,
      const std::vector<uint8>& verification_key_id,
      easy_unlock_crypto::ServiceImpl::EncryptionType encryption_type,
      easy_unlock_crypto::ServiceImpl::SignatureType signature_type) OVERRIDE;
  virtual std::vector<uint8> UnwrapSecureMessage(
      const std::vector<uint8>& secure_message,
      const std::vector<uint8>& key,
      const std::vector<uint8>& associated_data,
      easy_unlock_crypto::ServiceImpl::EncryptionType encryption_type,
      easy_unlock_crypto::ServiceImpl::SignatureType signature_type) OVERRIDE;

 private:
  int private_key_count_;
  int public_key_count_;

  DISALLOW_COPY_AND_ASSIGN(FakeService);
};

}  // namespace easy_unlock

#endif  // EASY_UNLOCK_FAKE_EASY_UNLOCK_SERVICE_H_
