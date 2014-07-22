// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "easy-unlock/easy_unlock_service.h"

#include <base/memory/scoped_ptr.h>
#include <chromeos/dbus/service_constants.h>
#include <easy-unlock-crypto/service_impl.h>

namespace {

class ServiceImpl : public easy_unlock::Service {
 public:
  ServiceImpl() : crypto_service_(new easy_unlock_crypto::ServiceImpl()) {}
  virtual ~ServiceImpl() {}

  virtual void GenerateEcP256KeyPair(std::vector<uint8>* private_key,
                                     std::vector<uint8>* public_key) OVERRIDE {
    crypto_service_->GenerateEcP256KeyPair(private_key, public_key);
  }

  virtual std::vector<uint8> PerformECDHKeyAgreement(
      const std::vector<uint8>& private_key,
      const std::vector<uint8>& public_key) OVERRIDE {
    return crypto_service_->PerformECDHKeyAgreement(private_key, public_key);
  }

  virtual std::vector<uint8> CreateSecureMessage(
      const std::vector<uint8>& payload,
      const std::vector<uint8>& key,
      const std::vector<uint8>& associated_data,
      const std::vector<uint8>& public_metadata,
      const std::vector<uint8>& verification_key_id,
      easy_unlock_crypto::ServiceImpl::EncryptionType encryption_type,
      easy_unlock_crypto::ServiceImpl::SignatureType signature_type) OVERRIDE {
    return crypto_service_->CreateSecureMessage(payload,
                                                key,
                                                associated_data,
                                                public_metadata,
                                                verification_key_id,
                                                encryption_type,
                                                signature_type);
  }

  virtual std::vector<uint8> UnwrapSecureMessage(
      const std::vector<uint8>& secure_message,
      const std::vector<uint8>& key,
      const std::vector<uint8>& associated_data,
      easy_unlock_crypto::ServiceImpl::EncryptionType encryption_type,
      easy_unlock_crypto::ServiceImpl::SignatureType signature_type) OVERRIDE {
    return crypto_service_->UnwrapSecureMessage(secure_message,
                                                key,
                                                associated_data,
                                                encryption_type,
                                                signature_type);
  }

 private:
  scoped_ptr<easy_unlock_crypto::ServiceImpl> crypto_service_;

  DISALLOW_COPY_AND_ASSIGN(ServiceImpl);
};

}  // namespace

namespace easy_unlock {

// static
Service* Service::Create() {
  return new ServiceImpl();
}

}  // namespace easy_unlock
