// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "trunks/authorization_session_impl.h"

#include <string>

#include <base/logging.h>
#include <base/macros.h>
#include <base/stl_util.h>
#include <crypto/scoped_openssl_types.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>

#include "trunks/error_codes.h"
#include "trunks/tpm_generated.h"
#include "trunks/tpm_utility.h"

namespace {

const size_t kWellKnownExponent = 0x10001;

}  // namespace

namespace trunks {

AuthorizationSessionImpl::AuthorizationSessionImpl(
    const TrunksFactory& factory) : factory_(factory) {}

AuthorizationSessionImpl::~AuthorizationSessionImpl() {
  if (!hmac_handle_) {
    return;
  }
  Tpm* tpm = factory_.GetTpm();
  TPM_RC result = tpm->FlushContextSync(hmac_handle_,
                                        "",
                                        NULL);
  if (result != TPM_RC_SUCCESS) {
    LOG(WARNING) << "Error closing authorization session: "
                 << GetErrorString(result);
  }
}

AuthorizationDelegate* AuthorizationSessionImpl::GetDelegate() {
  if (!hmac_handle_) {
    return NULL;
  }
  return &hmac_delegate_;
}

TPM_RC AuthorizationSessionImpl::StartBoundSession(
    TPMI_DH_ENTITY bind_entity,
    const std::string& bind_authorization_value,
    bool enable_encryption) {
  // First we generate a cryptographically secure salt and encrypt it using
  // PKCS1_OAEP padded RSA public key encryption. This is specified in TPM2.0
  // Part1 Architecture, Appendix B.10.2.
  std::string salt(SHA256_DIGEST_SIZE, 0);
  unsigned char* salt_buffer =
      reinterpret_cast<unsigned char*>(string_as_array(&salt));
  CHECK_EQ(RAND_bytes(salt_buffer, salt.size()), 1)
      << "Error generating a cryptographically random salt.";
  std::string encrypted_salt;
  TPM_RC salt_result = EncryptSalt(salt, &encrypted_salt);
  if (salt_result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error encrypting salt.";
    return TPM_RC_FAILURE;
  }
  TPM_HANDLE salt_handle = kSaltingKey;
  TPM2B_ENCRYPTED_SECRET encrypted_secret =
      Make_TPM2B_ENCRYPTED_SECRET(encrypted_salt);
  // Then we use TPM2_StartAuthSession to start a HMAC session with the TPM.
  // The tpm returns the tpm_nonce and the session_handle referencing the
  // created session.
  TPM_HANDLE session_handle;
  TPM2B_NONCE nonce_tpm;
  TPM_SE session_type = TPM_SE_HMAC;
  TPMI_ALG_HASH hash_algorithm = TPM_ALG_SHA256;
  TPMT_SYM_DEF symmetric_algorithm;
  symmetric_algorithm.algorithm = TPM_ALG_AES;
  symmetric_algorithm.key_bits.aes = 128;
  symmetric_algorithm.mode.aes = TPM_ALG_CFB;
  TPM2B_NONCE nonce_caller;
  // We use sha1_digest_size here because that is the minimum length
  // needed for the nonce.
  nonce_caller.size = SHA1_DIGEST_SIZE;
  CHECK_EQ(RAND_bytes(nonce_caller.buffer, nonce_caller.size), 1)
      << "Error generating a cryptographically random nonce.";
  Tpm* tpm = factory_.GetTpm();
  // The TPM2 command below needs no authorization. This is why we can use
  // the empty string "", when referring to the handle names for the salting
  // key and the bind entity.
  TPM_RC tpm_result = tpm->StartAuthSessionSync(salt_handle,
                                                "",  // salt_handle_name.
                                                bind_entity,
                                                "",  // bind_entity_name.
                                                nonce_caller,
                                                encrypted_secret,
                                                session_type,
                                                symmetric_algorithm,
                                                hash_algorithm,
                                                &session_handle,
                                                &nonce_tpm,
                                                NULL);  // No Authorization.
  if (tpm_result) {
    LOG(ERROR) << "Error creating an authorization session: "
               << GetErrorString(tpm_result);
    return tpm_result;
  }
  // Using the salt we generated and encrypted, and the data we got from the
  // TPM, we can initialize a HmacAuthorizationDelegate class.
  bool hmac_result = hmac_delegate_.InitSession(session_handle,
                                                nonce_tpm,
                                                nonce_caller,
                                                salt,
                                                bind_authorization_value,
                                                enable_encryption);
  if (!hmac_result) {
    LOG(ERROR) << "Failed to initialize an authorization session delegate.";
    return TPM_RC_FAILURE;
  }
  hmac_handle_ = session_handle;
  return TPM_RC_SUCCESS;
}

TPM_RC AuthorizationSessionImpl::StartUnboundSession(bool enable_encryption) {
  // Starting an unbound session is the same as starting a session bound to
  // TPM_RH_NULL. In this case, the authorization is the zero length buffer.
  // We can therefore simply call StartBoundSession with TPM_RH_NULL as the
  // binding entity, and the empty string as the authorization.
  return StartBoundSession(TPM_RH_NULL, "", enable_encryption);
}

void AuthorizationSessionImpl::SetEntityAuthorizationValue(
    const std::string& value) {
  hmac_delegate_.set_entity_auth_value(value);
}

void AuthorizationSessionImpl::SetFutureAuthorizationValue(
    const std::string& value) {
  hmac_delegate_.set_future_authorization_value(value);
}

TPM_RC AuthorizationSessionImpl::EncryptSalt(const std::string& salt,
                                             std::string* encrypted_salt) {
  TPM2B_NAME out_name;
  TPM2B_NAME qualified_name;
  TPM2B_PUBLIC public_data;
  public_data.public_area.unique.rsa.size = 0;
  // The TPM2 Command below needs no authorization. Therefore we can simply
  // use the empty string for all the Key names, and NULL for the authorization
  // delegate.
  TPM_RC result = factory_.GetTpm()->ReadPublicSync(kSaltingKey,
                                                    "",  // SaltingKey name.
                                                    &public_data,
                                                    &out_name,
                                                    &qualified_name,
                                                    NULL);  // No authorization
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error fetching salting key public info.";
    return result;
  }
  crypto::ScopedRSA salting_rsa(RSA_new());
  salting_rsa.get()->e = BN_new();
  if (!salting_rsa.get()->e) {
    LOG(ERROR) << "Error creating exponent for RSA.";
    return TPM_RC_FAILURE;
  }
  BN_set_word(salting_rsa.get()->e, kWellKnownExponent);
  salting_rsa.get()->n = BN_bin2bn(public_data.public_area.unique.rsa.buffer,
                                   public_data.public_area.unique.rsa.size,
                                   NULL);
  if (!salting_rsa.get()->n) {
    LOG(ERROR) << "Error setting public area of rsa key.";
    return TPM_RC_FAILURE;
  }
  // Label for RSAES-OAEP. Defined in TPM2.0 Part1 Architecture,
  // Appendix B.10.2.
  unsigned char oaep_param[7] = {'S', 'E', 'C', 'R', 'E', 'T', '\0'};
  std::string padded_input(RSA_size(salting_rsa.get()), 0);
  int rsa_result = RSA_padding_add_PKCS1_OAEP(
      reinterpret_cast<unsigned char*>(string_as_array(&padded_input)),
      padded_input.size(),
      reinterpret_cast<const unsigned char*>(salt.c_str()),
      salt.size(),
      oaep_param,
      arraysize(oaep_param));
  if (!rsa_result) {
    unsigned long err = ERR_get_error();  // NOLINT openssl types
    ERR_load_ERR_strings();
    ERR_load_crypto_strings();
    LOG(ERROR) << "EncryptSalt Error: " << err
               << ": " << ERR_lib_error_string(err)
               << ", " << ERR_func_error_string(err)
               << ", " << ERR_reason_error_string(err);
    return TPM_RC_FAILURE;
  }
  encrypted_salt->resize(padded_input.size());
  rsa_result = RSA_public_encrypt(
      padded_input.size(),
      reinterpret_cast<unsigned char*>(string_as_array(&padded_input)),
      reinterpret_cast<unsigned char*>(string_as_array(encrypted_salt)),
      salting_rsa.get(),
      RSA_NO_PADDING);
  if (rsa_result == -1) {
    unsigned long err = ERR_get_error();  // NOLINT openssl types
    ERR_load_ERR_strings();
    ERR_load_crypto_strings();
    LOG(ERROR) << "EncryptSalt Error: " << err
               << ": " << ERR_lib_error_string(err)
               << ", " << ERR_func_error_string(err)
               << ", " << ERR_reason_error_string(err);
    return TPM_RC_FAILURE;
  }
  return TPM_RC_SUCCESS;
}

}  // namespace trunks
