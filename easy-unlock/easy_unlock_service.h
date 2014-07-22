// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EASY_UNLOCK_EASY_UNLOCK_SERVICE_H_
#define EASY_UNLOCK_EASY_UNLOCK_SERVICE_H_

#include <string>
#include <vector>

#include <base/basictypes.h>
#include <easy-unlock-crypto/service_impl.h>

namespace easy_unlock {

// Wrapper around actual EasyUnlock dbus service implementation.
// See ServiceImpl in easy_unlock_crypto repo for more information on the
// methods provided by this interface.
class Service {
 public:
  // Creates the service implementation to be used in production code.
  // Caller should take ownership.
  static Service* Create();

  virtual ~Service() {}

  virtual void GenerateEcP256KeyPair(std::vector<uint8>* private_key,
                                     std::vector<uint8>* public_key) = 0;
  virtual std::vector<uint8> PerformECDHKeyAgreement(
      const std::vector<uint8>& private_key,
      const std::vector<uint8>& public_key) = 0;
  virtual std::vector<uint8> CreateSecureMessage(
      const std::vector<uint8>& payload,
      const std::vector<uint8>& key,
      const std::vector<uint8>& associated_data,
      const std::vector<uint8>& public_metadata,
      const std::vector<uint8>& verification_key_id,
      easy_unlock_crypto::ServiceImpl::EncryptionType encryption_type,
      easy_unlock_crypto::ServiceImpl::SignatureType signature_type) = 0;
  virtual std::vector<uint8> UnwrapSecureMessage(
      const std::vector<uint8>& secure_message,
      const std::vector<uint8>& key,
      const std::vector<uint8>& associated_data,
      easy_unlock_crypto::ServiceImpl::EncryptionType encryption_type,
      easy_unlock_crypto::ServiceImpl::SignatureType signature_type) = 0;
};

}  // namespace easy_unlock

#endif  // EASY_UNLOCK_EASY_UNLOCK_SERVICE_H_
