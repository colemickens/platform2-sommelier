// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "easy-unlock/fake_easy_unlock_service.h"

#include <base/file_util.h>
#include <base/strings/stringprintf.h>

namespace {

const char kSecureMessageTemplate[] =
    "securemessage:{"
      "payload:%s,"
      "key:%s,"
      "associated_data:%s,"
      "public_metadata:%s,"
      "verification_key_id:%s,"
      "encryption:%s,"
      "signature:%s"
    "}";

const char kUnwrappedMessageTemplate[] =
    "unwrappedmessage:{"
      "original:%s,"
      "key:%s,"
      "associated_data:%s,"
      "encryption:%s,"
      "signature:%s"
    "}";

std::string Uint8VectorAsString(const std::vector<uint8>& data) {
  return std::string(reinterpret_cast<const char*>(data.data()), data.size());
}

std::vector<uint8> StringAsUint8Vector(const std::string& data) {
  return std::vector<uint8>(data.c_str(), data.c_str() + data.length());
}

std::string EncryptionTypeAsString(
    easy_unlock_crypto::ServiceImpl::EncryptionType type) {
  switch (type) {
    case easy_unlock_crypto::ServiceImpl::ENCRYPTION_TYPE_NONE:
      return "NONE";
    case easy_unlock_crypto::ServiceImpl::ENCRYPTION_TYPE_AES_256_CBC:
      return "AES";
    default:
      return "";
  }
}

std::string SignatureTypeAsString(
    easy_unlock_crypto::ServiceImpl::SignatureType type) {
  switch (type) {
    case easy_unlock_crypto::ServiceImpl::SIGNATURE_TYPE_ECDSA_P256_SHA256:
      return "ECDSA_P256";
    case easy_unlock_crypto::ServiceImpl::SIGNATURE_TYPE_HMAC_SHA256:
      return "HMAC";
    default:
      return "";
  }
}

}  // namespace

namespace easy_unlock {

FakeService::FakeService() : private_key_count_(0),
                             public_key_count_(0) {
}

FakeService::~FakeService() {}

void FakeService::GenerateEcP256KeyPair(std::vector<uint8>* private_key,
                                        std::vector<uint8>* public_key) {
  *private_key = StringAsUint8Vector(
      base::StringPrintf("private_key_%d", ++private_key_count_));
  *public_key = StringAsUint8Vector(
      base::StringPrintf("public_key_%d", ++public_key_count_));
}

std::vector<uint8> FakeService::PerformECDHKeyAgreement(
    const std::vector<uint8>& private_key,
    const std::vector<uint8>& public_key) {
  return StringAsUint8Vector(base::StringPrintf(
      "secret_key:{private_key:%s,public_key:%s}",
       Uint8VectorAsString(private_key).c_str(),
       Uint8VectorAsString(public_key).c_str()));
}

std::vector<uint8> FakeService::CreateSecureMessage(
    const std::vector<uint8>& payload,
    const std::vector<uint8>& key,
    const std::vector<uint8>& associated_data,
    const std::vector<uint8>& public_metadata,
    const std::vector<uint8>& verification_key_id,
    easy_unlock_crypto::ServiceImpl::EncryptionType encryption_type,
    easy_unlock_crypto::ServiceImpl::SignatureType signature_type) {
  return StringAsUint8Vector(base::StringPrintf(
      kSecureMessageTemplate,
      Uint8VectorAsString(payload).c_str(),
      Uint8VectorAsString(key).c_str(),
      Uint8VectorAsString(associated_data).c_str(),
      Uint8VectorAsString(public_metadata).c_str(),
      Uint8VectorAsString(verification_key_id).c_str(),
      EncryptionTypeAsString(encryption_type).c_str(),
      SignatureTypeAsString(signature_type).c_str()));
}

std::vector<uint8> FakeService::UnwrapSecureMessage(
    const std::vector<uint8>& secure_message,
    const std::vector<uint8>& key,
    const std::vector<uint8>& associated_data,
    easy_unlock_crypto::ServiceImpl::EncryptionType encryption_type,
    easy_unlock_crypto::ServiceImpl::SignatureType signature_type) {
  return StringAsUint8Vector(base::StringPrintf(
      kUnwrappedMessageTemplate,
      Uint8VectorAsString(secure_message).c_str(),
      Uint8VectorAsString(key).c_str(),
      Uint8VectorAsString(associated_data).c_str(),
      EncryptionTypeAsString(encryption_type).c_str(),
      SignatureTypeAsString(signature_type).c_str()));
}

}  // namespace easy_unlock
