// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "trunks/hmac_auth_delegate.h"

#include <base/logging.h>
#include <base/memory/scoped_ptr.h>
#include <base/stl_util.h>
#include <crypto/secure_util.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>

namespace trunks {

namespace {

const uint32_t kDigestBits = 256;
const uint16_t kNonceMinSize = 16;
const uint16_t kNonceMaxSize = 32;
const uint8_t kDecryptSession = 1<<5;
const uint8_t kEncryptSession = 1<<6;
const std::string kAuthorizationKDFLabel = "ATH";

}  // namespace

HmacAuthDelegate::HmacAuthDelegate(bool parameter_encryption) {
  session_handle_ = 0;
  attributes_ = kContinueSession;
  if (parameter_encryption) {
    attributes_ |= kDecryptSession;
    attributes_ |= kEncryptSession;
  }
}

HmacAuthDelegate::~HmacAuthDelegate() {}

bool HmacAuthDelegate::GetCommandAuthorization(
    const std::string& command_hash,
    std::string* authorization) {
  if (!session_handle_) {
    authorization->clear();
    LOG(ERROR) << "Delegate being used before Initialization,";
    return false;
  }

  TPMS_AUTH_COMMAND auth;
  auth.session_handle = session_handle_;
  RegenerateCallerNonce();
  auth.nonce = caller_nonce_;
  auth.session_attributes = attributes_;
  std::string attributes_bytes;
  CHECK_EQ(Serialize_TPMA_SESSION(attributes_, &attributes_bytes),
           TPM_RC_SUCCESS) << "Error serializing session attributes.";

  std::string hmac_key = session_key_ + entity_auth_value_;
  std::string data;
  data.append(command_hash);
  data.append(reinterpret_cast<const char*>(caller_nonce_.buffer),
              caller_nonce_.size);
  data.append(reinterpret_cast<const char*>(tpm_nonce_.buffer),
              tpm_nonce_.size);
  data.append(attributes_bytes);
  std::string digest = HmacSha256(hmac_key, data);
  auth.hmac = Make_TPM2B_DIGEST(digest);

  TPM_RC serialize_error = Serialize_TPMS_AUTH_COMMAND(auth, authorization);
  if (serialize_error != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Could not serialize command auth.";
    return false;
  }
  return true;
}

bool HmacAuthDelegate::CheckResponseAuthorization(
    const std::string& response_hash,
    const std::string& authorization) {
  if (!session_handle_) {
    return false;
  }

  TPMS_AUTH_RESPONSE auth_response;
  std::string mutable_auth_string(authorization);
  TPM_RC parse_error;
  parse_error = Parse_TPMS_AUTH_RESPONSE(&mutable_auth_string,
                                         &auth_response,
                                         NULL);
  if (parse_error != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Could not parse authorization response.";
    return false;
  }
  if (auth_response.hmac.size != kHashDigestSize) {
    LOG(ERROR) << "TPM auth hmac was incorrect size.";
    return false;
  }
  if (auth_response.nonce.size < 16 || auth_response.nonce.size > 32) {
    LOG(ERROR) << "TPM_nonce is not the correct length.";
    return false;
  }
  if ((auth_response.session_attributes & ~kContinueSession) !=
      (attributes_ & ~kContinueSession)) {
    LOG(ERROR) << "TPM attributes were incorrect.";
    return false;
  }
  tpm_nonce_ = auth_response.nonce;
  std::string attributes_bytes;
  CHECK_EQ(Serialize_TPMA_SESSION(attributes_, &attributes_bytes),
           TPM_RC_SUCCESS) << "Error serializing session attributes.";

  std::string hmac_key = session_key_ + entity_auth_value_;
  std::string data;
  data.append(response_hash);
  data.append(reinterpret_cast<const char*>(tpm_nonce_.buffer),
              tpm_nonce_.size);
  data.append(reinterpret_cast<const char*>(caller_nonce_.buffer),
              caller_nonce_.size);
  data.append(attributes_bytes);
  std::string digest = HmacSha256(hmac_key, data);
  CHECK_EQ(digest.size(), kHashDigestSize);
  if (!crypto::SecureMemEqual(digest.data(), auth_response.hmac.buffer,
                              digest.size())) {
    LOG(ERROR) << "Authorization response hash did not match expected value.";
    return false;
  }
  return true;
}

bool HmacAuthDelegate::EncryptCommandParameter(std::string* parameter) {
  if (!session_handle_) {
    return false;
  }
  if (!(attributes_ & kEncryptSession)) {
    // No parameter encryption enabled.
    return true;
  }
  // TODO(usanghi): implement encryption
  return true;
}

bool HmacAuthDelegate::DecryptResponseParameter(std::string* parameter) {
  if (!session_handle_) {
    return false;
  }
  if (!(attributes_ & kDecryptSession)) {
    // No parameter decryption enabled.
    return true;
  }
  // TODO(usanghi): implement decryption
  return true;
}

bool HmacAuthDelegate::InitSession(TPM_HANDLE session_handle,
                                   const TPM2B_NONCE& tpm_nonce,
                                   const TPM2B_NONCE& caller_nonce,
                                   const std::string& salt,
                                   const std::string& bind_auth_value) {
  session_handle_ = session_handle;
  if (caller_nonce.size < kNonceMinSize || caller_nonce.size > kNonceMaxSize ||
      tpm_nonce.size < kNonceMinSize || tpm_nonce.size > kNonceMaxSize) {
    LOG(INFO) << "Session Nonces have to be between 16 and 32 bytes long.";
    return false;
  }
  tpm_nonce_ = tpm_nonce;
  caller_nonce_ = caller_nonce;
  session_key_ = CreateKey(bind_auth_value + salt, kAuthorizationKDFLabel,
                           tpm_nonce_, caller_nonce_);
  return true;
}

void HmacAuthDelegate::set_entity_auth_value(const std::string& auth_value) {
  entity_auth_value_ = auth_value;
}

std::string HmacAuthDelegate::CreateKey(const std::string& hmac_key,
                                        const std::string& label,
                                        const TPM2B_NONCE& nonce_newer,
                                        const TPM2B_NONCE& nonce_older) {
  if (hmac_key.length() == 0) {
    LOG(INFO) << "No sessionKey generated for unsalted and unbound session.";
    return std::string();
  }

  std::string counter;
  std::string digest_size_bits;
  if (Serialize_uint32_t(0, &counter) != TPM_RC_SUCCESS ||
      Serialize_uint32_t(kDigestBits, &digest_size_bits) != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error serializing uint32_t during session key generation.";
    return std::string();
  }
  CHECK_EQ(counter.size(), sizeof(uint32_t));
  CHECK_EQ(digest_size_bits.size(), sizeof(uint32_t));

  std::string data;
  data.append(counter);
  data.append(label);
  data.append(reinterpret_cast<const char*>(nonce_newer.buffer),
              nonce_newer.size);
  data.append(reinterpret_cast<const char*>(nonce_older.buffer),
              nonce_older.size);
  data.append(digest_size_bits);
  std::string key = HmacSha256(hmac_key, data);
  return key;
}

std::string HmacAuthDelegate::HmacSha256(const std::string& key,
                                          const std::string& data) {
  unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int digest_length;
  HMAC(EVP_sha256(),
       key.data(),
       key.size(),
       reinterpret_cast<const unsigned char*>(data.data()),
       data.size(),
       digest,
       &digest_length);
  CHECK_EQ(digest_length, kHashDigestSize);
  return std::string(reinterpret_cast<char*>(digest), digest_length);
}

void HmacAuthDelegate::RegenerateCallerNonce() {
  CHECK(session_handle_);
  // RAND_bytes takes a signed number, but since nonce_size is guaranteed to be
  // less than 32 bytes and greater than 16 we dont have to worry about it.
  CHECK_EQ(RAND_bytes(caller_nonce_.buffer, caller_nonce_.size), 1) <<
      "Error regnerating a cryptographically random nonce.";
}

}  // namespace trunks
