// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "trunks/session_manager_impl.h"

#include <string>

#include <base/logging.h>
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

SessionManagerImpl::SessionManagerImpl(const TrunksFactory& factory)
    : factory_(factory),
      session_handle_(kUninitializedHandle) {}

SessionManagerImpl::~SessionManagerImpl() {
  CloseSession();
}

void SessionManagerImpl::CloseSession() {
  if (session_handle_ == kUninitializedHandle) {
    return;
  }
  TPM_RC result = factory_.GetTpm()->FlushContextSync(session_handle_, nullptr);
  if (result != TPM_RC_SUCCESS) {
    LOG(WARNING) << "Error closing tpm session: " << GetErrorString(result);
  }
  session_handle_ = kUninitializedHandle;
}

TPM_RC SessionManagerImpl::StartSession(
    TPM_SE session_type,
    TPMI_DH_ENTITY bind_entity,
    const std::string& bind_authorization_value,
    bool enable_encryption,
    HmacAuthorizationDelegate* delegate) {
  CHECK(delegate);
  // If we already have an active session, close it.
  CloseSession();

  std::string salt(SHA256_DIGEST_SIZE, 0);
  unsigned char* salt_buffer =
      reinterpret_cast<unsigned char*>(string_as_array(&salt));
  CHECK_EQ(RAND_bytes(salt_buffer, salt.size()), 1)
      << "Error generating a cryptographically random salt.";
  // First we enccrypt the cryptographically secure salt using PKCS1_OAEP
  // padded RSA public key encryption. This is specified in TPM2.0
  // Part1 Architecture, Appendix B.10.2.
  std::string encrypted_salt;
  TPM_RC salt_result = EncryptSalt(salt, &encrypted_salt);
  if (salt_result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error encrypting salt.";
    return salt_result;
  }

  TPM2B_ENCRYPTED_SECRET encrypted_secret =
      Make_TPM2B_ENCRYPTED_SECRET(encrypted_salt);
  // Then we use TPM2_StartAuthSession to start a HMAC session with the TPM.
  // The tpm returns the tpm_nonce and the session_handle referencing the
  // created session.
  TPMI_ALG_HASH hash_algorithm = TPM_ALG_SHA256;
  TPMT_SYM_DEF symmetric_algorithm;
  symmetric_algorithm.algorithm = TPM_ALG_AES;
  symmetric_algorithm.key_bits.aes = 128;
  symmetric_algorithm.mode.aes = TPM_ALG_CFB;

  TPM2B_NONCE nonce_caller;
  TPM2B_NONCE nonce_tpm;
  // We use sha1_digest_size here because that is the minimum length
  // needed for the nonce.
  nonce_caller.size = SHA1_DIGEST_SIZE;
  CHECK_EQ(RAND_bytes(nonce_caller.buffer, nonce_caller.size), 1)
      << "Error generating a cryptographically random nonce.";

  Tpm* tpm = factory_.GetTpm();
  // The TPM2 command below needs no authorization. This is why we can use
  // the empty string "", when referring to the handle names for the salting
  // key and the bind entity.
  TPM_RC tpm_result = tpm->StartAuthSessionSync(kSaltingKey,
                                                "",  // salt_handle_name.
                                                bind_entity,
                                                "",  // bind_entity_name.
                                                nonce_caller,
                                                encrypted_secret,
                                                session_type,
                                                symmetric_algorithm,
                                                hash_algorithm,
                                                &session_handle_,
                                                &nonce_tpm,
                                                nullptr);  // No Authorization.
  if (tpm_result) {
    LOG(ERROR) << "Error creating an authorization session: "
               << GetErrorString(tpm_result);
    return tpm_result;
  }
  bool hmac_result = delegate->InitSession(
    session_handle_,
    nonce_tpm,
    nonce_caller,
    salt,
    bind_authorization_value,
    enable_encryption);
  if (!hmac_result) {
    LOG(ERROR) << "Failed to initialize an authorization session delegate.";
    return TPM_RC_FAILURE;
  }
  return TPM_RC_SUCCESS;
}

TPM_RC SessionManagerImpl::EncryptSalt(const std::string& salt,
                                       std::string* encrypted_salt) {
  TPM2B_NAME out_name;
  TPM2B_NAME qualified_name;
  TPM2B_PUBLIC public_data;
  public_data.public_area.unique.rsa.size = 0;
  // The TPM2 Command below needs no authorization. Therefore we can simply
  // use the empty string for all the Key names, and pullptr for the
  // authorization delegate.
  TPM_RC result = factory_.GetTpm()->ReadPublicSync(
      kSaltingKey, "",  // SaltingKey name.
      &public_data, &out_name, &qualified_name, nullptr);  // No authorization
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
                                   nullptr);
  if (!salting_rsa.get()->n) {
    LOG(ERROR) << "Error setting public area of rsa key.";
    return TPM_RC_FAILURE;
  }
  // Label for RSAES-OAEP. Defined in TPM2.0 Part1 Architecture,
  // Appendix B.10.2.
  unsigned char oaep_param[7] = {'S', 'E', 'C', 'R', 'E', 'T', '\0'};
  const EVP_MD* sha256_md = EVP_sha256();
  std::string padded_input(RSA_size(salting_rsa.get()), 0);
  int rsa_result = RSA_padding_add_PKCS1_OAEP_mgf1(
      reinterpret_cast<unsigned char*>(string_as_array(&padded_input)),
      padded_input.size(),
      reinterpret_cast<const unsigned char*>(salt.c_str()),
      salt.size(),
      oaep_param,
      arraysize(oaep_param),
      sha256_md,
      sha256_md);
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
