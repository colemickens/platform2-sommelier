// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "trunks/tpm_utility_impl.h"

#include <base/logging.h>
#include <base/memory/scoped_ptr.h>
#include <base/stl_util.h>
#include <crypto/secure_hash.h>
#include <crypto/sha2.h>

#include "trunks/authorization_delegate.h"
#include "trunks/authorization_session.h"
#include "trunks/error_codes.h"
#include "trunks/null_authorization_delegate.h"
#include "trunks/tpm_state.h"
#include "trunks/trunks_factory.h"

namespace {

const char kPlatformPassword[] = "cros-platform";
const trunks::TPMA_OBJECT kFixedTPM = 1U << 1;
const trunks::TPMA_OBJECT kStClear = 1U << 2;
const trunks::TPMA_OBJECT kFixedParent = 1U << 4;
const trunks::TPMA_OBJECT kSensitiveDataOrigin = 1U << 5;
const trunks::TPMA_OBJECT kUserWithAuth = 1U << 6;
const trunks::TPMA_OBJECT kNoDA = 1U << 10;
const trunks::TPMA_OBJECT kRestricted = 1U << 16;
const trunks::TPMA_OBJECT kDecrypt = 1U << 17;
const trunks::TPMA_OBJECT kSign = 1U << 18;

// Returns a serialized representation of the unmodified handle. This is useful
// for predefined handle values, like TPM_RH_OWNER. For details on what types of
// handles use this name formula see Table 3 in the TPM 2.0 Library Spec Part 1
// (Section 16 - Names).
std::string NameFromHandle(trunks::TPM_HANDLE handle) {
  std::string name;
  trunks::Serialize_TPM_HANDLE(handle, &name);
  return name;
}

}  // namespace

namespace trunks {

TpmUtilityImpl::TpmUtilityImpl(const TrunksFactory& factory)
    : factory_(factory) {
  session_ = factory_.GetAuthorizationSession();
}

TpmUtilityImpl::~TpmUtilityImpl() {
}

TPM_RC TpmUtilityImpl::Startup() {
  TPM_RC result = TPM_RC_SUCCESS;
  Tpm* tpm = factory_.GetTpm();
  NullAuthorizationDelegate authorization;
  result = tpm->StartupSync(TPM_SU_CLEAR, &authorization);
  // Ignore TPM_RC_INITIALIZE, that means it was already started.
  if (result && result != TPM_RC_INITIALIZE) {
    LOG(ERROR) << __func__ << ": " << GetErrorString(result);
    return result;
  }
  result = tpm->SelfTestSync(YES /* Full test. */, &authorization);
  if (result) {
    LOG(ERROR) << __func__ << ": " << GetErrorString(result);
    return result;
  }
  return TPM_RC_SUCCESS;
}

TPM_RC TpmUtilityImpl::InitializeTpm() {
  TPM_RC result = TPM_RC_SUCCESS;
  scoped_ptr<TpmState> tpm_state(factory_.GetTpmState());
  result = tpm_state->Initialize();
  if (result) {
    LOG(ERROR) << __func__ << ": " << GetErrorString(result);
    return result;
  }
  // Warn about various unexpected conditions.
  if (!tpm_state->WasShutdownOrderly()) {
    LOG(WARNING) << "WARNING: The last TPM shutdown was not orderly.";
  }
  if (tpm_state->IsInLockout()) {
    LOG(WARNING) << "WARNING: The TPM is currently in lockout.";
  }

  // We expect the firmware has already locked down the platform hierarchy. If
  // it hasn't, do it now.
  if (tpm_state->IsPlatformHierarchyEnabled()) {
    scoped_ptr<AuthorizationDelegate> empty_password(
        factory_.GetPasswordAuthorization(""));
    result = SetHierarchyAuthorization(TPM_RH_PLATFORM,
                                       kPlatformPassword,
                                       empty_password.get());
    if (GetFormatOneError(result) == TPM_RC_BAD_AUTH) {
      // Most likely the platform password has already been set.
      result = TPM_RC_SUCCESS;
    }
    if (result) {
      LOG(ERROR) << __func__ << ": " << GetErrorString(result);
      return result;
    }
    scoped_ptr<AuthorizationDelegate> authorization(
        factory_.GetPasswordAuthorization(kPlatformPassword));
    result = DisablePlatformHierarchy(authorization.get());
    if (result) {
      LOG(ERROR) << __func__ << ": " << GetErrorString(result);
      return result;
    }
  }
  return TPM_RC_SUCCESS;
}

TPM_RC TpmUtilityImpl::StirRandom(const std::string& entropy_data) {
  NullAuthorizationDelegate null_delegate;
  std::string digest = crypto::SHA256HashString(entropy_data);
  TPM2B_SENSITIVE_DATA random_bytes = Make_TPM2B_SENSITIVE_DATA(digest);
  return factory_.GetTpm()->StirRandomSync(random_bytes, &null_delegate);
}

TPM_RC TpmUtilityImpl::GenerateRandom(int num_bytes,
                                      std::string* random_data) {
  NullAuthorizationDelegate null_delegate;
  int bytes_left = num_bytes;
  random_data->clear();
  TPM_RC rc;
  TPM2B_DIGEST digest;
  while (bytes_left > 0) {
    rc = factory_.GetTpm()->GetRandomSync(bytes_left,
                                          &digest,
                                          &null_delegate);
    if (rc) {
      LOG(ERROR) << "Error getting random data from tpm.";
      return rc;
    }
    random_data->append(StringFrom_TPM2B_DIGEST(digest));
    bytes_left -= digest.size;
  }
  CHECK_EQ(random_data->size(), num_bytes);
  return TPM_RC_SUCCESS;
}

TPM_RC TpmUtilityImpl::ExtendPCR(int pcr_index,
                                 const std::string& extend_data) {
  NullAuthorizationDelegate null_delegate;
  if (pcr_index < 0 || pcr_index >= IMPLEMENTATION_PCR) {
    LOG(ERROR) << "Using a PCR index that isnt implemented.";
    return TPM_RC_FAILURE;
  }
  TPM_HANDLE pcr_handle = HR_PCR + pcr_index;
  std::string pcr_name = NameFromHandle(pcr_handle);
  TPML_DIGEST_VALUES digests;
  digests.count = 1;
  digests.digests[0].hash_alg = TPM_ALG_SHA256;
  crypto::SHA256HashString(extend_data,
                           digests.digests[0].digest.sha256,
                           crypto::kSHA256Length);
  return factory_.GetTpm()->PCR_ExtendSync(pcr_handle,
                                           pcr_name,
                                           digests,
                                           &null_delegate);
}

TPM_RC TpmUtilityImpl::ReadPCR(int pcr_index, std::string* pcr_value) {
  NullAuthorizationDelegate null_delegate;
  TPML_PCR_SELECTION pcr_select_in;
  uint32_t pcr_update_counter;
  TPML_PCR_SELECTION pcr_select_out;
  TPML_DIGEST pcr_values;
  // This process of selecting pcrs is highlighted in TPM 2.0 Library Spec
  // Part 2 (Section 10.5 - PCR structures).
  uint8_t pcr_select_index = pcr_index / 8;
  uint8_t pcr_select_byte = 1 << (pcr_index % 8);
  pcr_select_in.count = 1;
  pcr_select_in.pcr_selections[0].hash = TPM_ALG_SHA256;
  pcr_select_in.pcr_selections[0].sizeof_select = pcr_select_index + 1;
  pcr_select_in.pcr_selections[0].pcr_select[pcr_select_index] =
      pcr_select_byte;

  TPM_RC rc = factory_.GetTpm()->PCR_ReadSync(pcr_select_in,
                                              &pcr_update_counter,
                                              &pcr_select_out,
                                              &pcr_values,
                                              &null_delegate);
  if (rc) {
    LOG(INFO) << "Error trying to read a pcr: " << rc;
    return rc;
  }
  if (pcr_select_out.count != 1 ||
      pcr_select_out.pcr_selections[0].sizeof_select !=
      (pcr_select_index + 1) ||
      pcr_select_out.pcr_selections[0].pcr_select[pcr_select_index] !=
      pcr_select_byte) {
    LOG(ERROR) << "TPM did not return the requested PCR";
    return TPM_RC_FAILURE;
  }
  CHECK_GE(pcr_values.count, 1);
  pcr_value->assign(StringFrom_TPM2B_DIGEST(pcr_values.digests[0]));
  return TPM_RC_SUCCESS;
}

TPM_RC TpmUtilityImpl::TakeOwnership(const std::string& owner_password,
                                     const std::string& endorsement_password,
                                     const std::string& lockout_password) {
  TPM_RC result = InitializeSession();
  if (result) {
    LOG(ERROR) << __func__ << ": " << GetErrorString(result);
    return result;
  }
  scoped_ptr<TpmState> tpm_state(factory_.GetTpmState());
  result = tpm_state->Initialize();
  if (result) {
    LOG(ERROR) << __func__ << ": " << GetErrorString(result);
    return result;
  }
  if (!tpm_state->IsOwnerPasswordSet()) {
    result = SetHierarchyAuthorization(TPM_RH_OWNER,
                                       owner_password,
                                       session_->GetDelegate());
    if (result) {
      LOG(ERROR) << __func__ << ": " << GetErrorString(result);
      return result;
    }
  }
  if (!tpm_state->IsEndorsementPasswordSet()) {
    result = SetHierarchyAuthorization(TPM_RH_ENDORSEMENT,
                                       endorsement_password,
                                       session_->GetDelegate());
    if (result) {
      LOG(ERROR) << __func__ << ": " << GetErrorString(result);
      return result;
    }
  }
  if (!tpm_state->IsLockoutPasswordSet()) {
    result = SetHierarchyAuthorization(TPM_RH_LOCKOUT,
                                       lockout_password,
                                       session_->GetDelegate());
    if (result) {
      LOG(ERROR) << __func__ << ": " << GetErrorString(result);
      return result;
    }
  }
  return TPM_RC_SUCCESS;
}

TPM_RC TpmUtilityImpl::CreateStorageRootKeys(
    const std::string& owner_password) {
  TPM_RC result = InitializeSession();
  if (result) {
    LOG(ERROR) << __func__ << ": " << GetErrorString(result);
    return result;
  }
  Tpm* tpm = factory_.GetTpm();
  session_->SetEntityAuthorizationValue(owner_password);
  TPM2B_PUBLIC rsa_public_area;
  rsa_public_area.size = sizeof(TPMT_PUBLIC);
  rsa_public_area.public_area.type = TPM_ALG_RSA;
  rsa_public_area.public_area.name_alg = TPM_ALG_SHA256;
  rsa_public_area.public_area.object_attributes =
      kFixedTPM | kStClear | kFixedParent | kSensitiveDataOrigin |
      kUserWithAuth | kNoDA | kRestricted | kDecrypt;
  rsa_public_area.public_area.auth_policy = Make_TPM2B_DIGEST("");
  rsa_public_area.public_area.parameters.rsa_detail.symmetric.algorithm =
      TPM_ALG_AES;
  rsa_public_area.public_area.parameters.rsa_detail.symmetric.key_bits.aes =
      128;
  rsa_public_area.public_area.parameters.rsa_detail.symmetric.mode.aes =
      TPM_ALG_CFB;
  rsa_public_area.public_area.parameters.rsa_detail.scheme.scheme =
      TPM_ALG_NULL;
  rsa_public_area.public_area.parameters.rsa_detail.key_bits = 2048;
  rsa_public_area.public_area.parameters.rsa_detail.exponent = 0;
  rsa_public_area.public_area.unique.rsa = Make_TPM2B_PUBLIC_KEY_RSA("");
  TPML_PCR_SELECTION creation_pcrs;
  creation_pcrs.count = 0;
  TPM2B_SENSITIVE_CREATE sensitive_create;
  sensitive_create.size = sizeof(TPMS_SENSITIVE_CREATE);
  sensitive_create.sensitive.user_auth = Make_TPM2B_DIGEST("");
  sensitive_create.sensitive.data = Make_TPM2B_SENSITIVE_DATA("");
  TPM_HANDLE object_handle;
  TPM2B_CREATION_DATA creation_data;
  TPM2B_DIGEST creation_digest;
  TPMT_TK_CREATION creation_ticket;
  TPM2B_NAME object_name;
  result = tpm->CreatePrimarySync(TPM_RH_OWNER,
                                  NameFromHandle(TPM_RH_OWNER),
                                  sensitive_create,
                                  rsa_public_area,
                                  Make_TPM2B_DATA(""),
                                  creation_pcrs,
                                  &object_handle,
                                  &rsa_public_area,
                                  &creation_data,
                                  &creation_digest,
                                  &creation_ticket,
                                  &object_name,
                                  session_->GetDelegate());
  if (result) {
    LOG(ERROR) << __func__ << ": " << GetErrorString(result);
    return result;
  }
  // This will make the key persistent.
  result = tpm->EvictControlSync(TPM_RH_OWNER,
                                 NameFromHandle(TPM_RH_OWNER),
                                 object_handle,
                                 StringFrom_TPM2B_NAME(object_name),
                                 kRSAStorageRootKey,
                                 session_->GetDelegate());
  if (result) {
    LOG(ERROR) << __func__ << ": " << GetErrorString(result);
    return result;
  }
  // Do it again for ECC.
  TPM2B_PUBLIC ecc_public_area;
  ecc_public_area.size = sizeof(TPMT_PUBLIC);
  ecc_public_area.public_area.type = TPM_ALG_ECC;
  ecc_public_area.public_area.name_alg = TPM_ALG_SHA256;
  ecc_public_area.public_area.object_attributes =
      kFixedTPM | kStClear | kFixedParent | kSensitiveDataOrigin |
      kUserWithAuth | kNoDA | kRestricted | kDecrypt;
  ecc_public_area.public_area.auth_policy = Make_TPM2B_DIGEST("");
  ecc_public_area.public_area.parameters.ecc_detail.symmetric.algorithm =
      TPM_ALG_AES;
  ecc_public_area.public_area.parameters.ecc_detail.symmetric.key_bits.aes =
      128;
  ecc_public_area.public_area.parameters.ecc_detail.symmetric.mode.aes =
      TPM_ALG_CFB;
  ecc_public_area.public_area.parameters.ecc_detail.scheme.scheme =
      TPM_ALG_NULL;
  ecc_public_area.public_area.parameters.ecc_detail.curve_id =
      TPM_ECC_NIST_P256;
  ecc_public_area.public_area.parameters.ecc_detail.kdf.scheme = TPM_ALG_MGF1;
  ecc_public_area.public_area.parameters.ecc_detail.kdf.details.mgf1.hash_alg =
      TPM_ALG_SHA256;
  ecc_public_area.public_area.unique.ecc.x = Make_TPM2B_ECC_PARAMETER("");
  ecc_public_area.public_area.unique.ecc.y = Make_TPM2B_ECC_PARAMETER("");
  result = tpm->CreatePrimarySync(TPM_RH_OWNER,
                                  NameFromHandle(TPM_RH_OWNER),
                                  sensitive_create,
                                  ecc_public_area,
                                  Make_TPM2B_DATA(""),
                                  creation_pcrs,
                                  &object_handle,
                                  &ecc_public_area,
                                  &creation_data,
                                  &creation_digest,
                                  &creation_ticket,
                                  &object_name,
                                  session_->GetDelegate());
  if (result) {
    LOG(ERROR) << __func__ << ": " << GetErrorString(result);
    return result;
  }
  // This will make the key persistent.
  result = tpm->EvictControlSync(TPM_RH_OWNER,
                                 NameFromHandle(TPM_RH_OWNER),
                                 object_handle,
                                 StringFrom_TPM2B_NAME(object_name),
                                 kECCStorageRootKey,
                                 session_->GetDelegate());
  if (result) {
    LOG(ERROR) << __func__ << ": " << GetErrorString(result);
    return result;
  }
  return TPM_RC_SUCCESS;
}

TPM_RC TpmUtilityImpl::AsymmetricEncrypt(TPM_HANDLE key_handle,
                                         TPM_ALG_ID scheme,
                                         TPM_ALG_ID hash_alg,
                                         const std::string& plaintext,
                                         std::string* ciphertext) {
  TPMT_RSA_DECRYPT in_scheme;
  TPM_ALG_ID hash_in;
  if (hash_alg == TPM_ALG_NULL) {
    hash_in = TPM_ALG_SHA256;
  } else {
    hash_in = hash_alg;
  }
  if (scheme == TPM_ALG_RSAES) {
    in_scheme.scheme = TPM_ALG_RSAES;
  } else if (scheme == TPM_ALG_OAEP || scheme == TPM_ALG_NULL) {
    in_scheme.scheme = TPM_ALG_OAEP;
    in_scheme.details.oaep.hash_alg = hash_in;
  } else {
    LOG(ERROR) << "Invalid Signing scheme used.";
    return SAPI_RC_BAD_PARAMETER;
  }

  TPM2B_PUBLIC public_area;
  TPM_RC return_code = GetKeyPublicArea(key_handle, &public_area);
  if (return_code) {
    LOG(ERROR) << "Error finding public area for: " << key_handle;
    return return_code;
  } else if (public_area.public_area.type != TPM_ALG_RSA) {
    LOG(ERROR) << "Key handle given is not an RSA key";
    return SAPI_RC_BAD_PARAMETER;
  } else if ((public_area.public_area.object_attributes & kDecrypt) == 0) {
    LOG(ERROR) << "Key handle given is not a decryption key";
    return SAPI_RC_BAD_PARAMETER;
  }
  if ((public_area.public_area.object_attributes & kRestricted) != 0) {
    LOG(ERROR) << "Cannot use RSAES for encryption with a restricted key";
    return SAPI_RC_BAD_PARAMETER;
  }
  std::string key_name;
  return_code = GetKeyName(key_handle, &key_name);
  if (return_code) {
    LOG(ERROR) << "Error finding key name for: " << key_handle;
    return return_code;
  }

  NullAuthorizationDelegate null_delegate;
  TPM2B_DATA label;
  label.size = 0;
  TPM2B_PUBLIC_KEY_RSA in_message = Make_TPM2B_PUBLIC_KEY_RSA(plaintext);
  TPM2B_PUBLIC_KEY_RSA out_message;
  return_code = factory_.GetTpm()->RSA_EncryptSync(key_handle,
                                                   key_name,
                                                   in_message,
                                                   in_scheme,
                                                   label,
                                                   &out_message,
                                                   &null_delegate);
  if (return_code) {
    LOG(ERROR) << "Error performing RSA encrypt: "
               << GetErrorString(return_code);
    return return_code;
  }
  ciphertext->assign(StringFrom_TPM2B_PUBLIC_KEY_RSA(out_message));
  return TPM_RC_SUCCESS;
}

TPM_RC TpmUtilityImpl::AsymmetricDecrypt(TPM_HANDLE key_handle,
                                         TPM_ALG_ID scheme,
                                         TPM_ALG_ID hash_alg,
                                         const std::string& password,
                                         const std::string& ciphertext,
                                         std::string* plaintext) {
  TPMT_RSA_DECRYPT in_scheme;
  TPM_ALG_ID hash_in;
  if (hash_alg == TPM_ALG_NULL) {
    hash_in = TPM_ALG_SHA256;
  } else {
    hash_in = hash_alg;
  }
  if (scheme == TPM_ALG_RSAES) {
    in_scheme.scheme = TPM_ALG_RSAES;
  } else if (scheme == TPM_ALG_OAEP || scheme == TPM_ALG_NULL) {
    in_scheme.scheme = TPM_ALG_OAEP;
    in_scheme.details.oaep.hash_alg = hash_in;
  } else {
    LOG(ERROR) << "Invalid Signing scheme used.";
    return SAPI_RC_BAD_PARAMETER;
  }

  TPM2B_PUBLIC public_area;
  TPM_RC return_code = GetKeyPublicArea(key_handle, &public_area);
  if (return_code) {
    LOG(ERROR) << "Error finding public area for: " << key_handle;
    return return_code;
  } else if (public_area.public_area.type != TPM_ALG_RSA) {
    LOG(ERROR) << "Key handle given is not an RSA key";
    return SAPI_RC_BAD_PARAMETER;
  } else if ((public_area.public_area.object_attributes & kDecrypt) == 0) {
    LOG(ERROR) << "Key handle given is not a decryption key";
    return SAPI_RC_BAD_PARAMETER;
  }
  if ((public_area.public_area.object_attributes & kRestricted) != 0) {
    LOG(ERROR) << "Cannot use RSAES for encryption with a restricted key";
    return SAPI_RC_BAD_PARAMETER;
  }
  std::string key_name;
  return_code = GetKeyName(key_handle, &key_name);
  if (return_code) {
    LOG(ERROR) << "Error finding key name for: " << key_handle;
    return return_code;
  }

  TPM2B_DATA label;
  label.size = 0;
  TPM2B_PUBLIC_KEY_RSA in_message = Make_TPM2B_PUBLIC_KEY_RSA(ciphertext);
  TPM2B_PUBLIC_KEY_RSA out_message;
  session_->SetEntityAuthorizationValue(password);
  return_code = factory_.GetTpm()->RSA_DecryptSync(key_handle,
                                                   key_name,
                                                   in_message,
                                                   in_scheme,
                                                   label,
                                                   &out_message,
                                                   session_->GetDelegate());
  if (return_code) {
    LOG(ERROR) << "Error performing RSA decrypt: "
               << GetErrorString(return_code);
    return return_code;
  }
  plaintext->assign(StringFrom_TPM2B_PUBLIC_KEY_RSA(out_message));
  return TPM_RC_SUCCESS;
}

TPM_RC TpmUtilityImpl::Sign(TPM_HANDLE key_handle,
                            TPM_ALG_ID scheme,
                            TPM_ALG_ID hash_alg,
                            const std::string& password,
                            const std::string& digest,
                            std::string* signature) {
  TPMT_SIG_SCHEME in_scheme;
  TPM_ALG_ID hash_in;
  if (hash_alg == TPM_ALG_NULL) {
    hash_in = TPM_ALG_SHA256;
  } else {
    hash_in = hash_alg;
  }
  if (scheme == TPM_ALG_RSAPSS) {
    in_scheme.scheme = TPM_ALG_RSAPSS;
    in_scheme.details.rsapss.hash_alg = hash_in;
  } else if (scheme == TPM_ALG_RSASSA || scheme == TPM_ALG_NULL) {
    in_scheme.scheme = TPM_ALG_RSASSA;
    in_scheme.details.rsassa.hash_alg = hash_in;
  } else {
    LOG(ERROR) << "Invalid Signing scheme used.";
    return SAPI_RC_BAD_PARAMETER;
  }

  TPM2B_PUBLIC public_area;
  TPM_RC return_code = GetKeyPublicArea(key_handle, &public_area);
  if (return_code) {
    LOG(ERROR) << "Error finding public area for: " << key_handle;
    return return_code;
  } else if (public_area.public_area.type != TPM_ALG_RSA) {
    LOG(ERROR) << "Key handle given is not an RSA key";
    return SAPI_RC_BAD_PARAMETER;
  } else if ((public_area.public_area.object_attributes & kSign) == 0) {
    LOG(ERROR) << "Key handle given is not a signging key";
    return SAPI_RC_BAD_PARAMETER;
  } else if ((public_area.public_area.object_attributes & kRestricted) != 0) {
    LOG(ERROR) << "Key handle references a restricted key";
    return SAPI_RC_BAD_PARAMETER;
  }

  std::string key_name;
  return_code = GetKeyName(key_handle, &key_name);
  if (return_code) {
    LOG(ERROR) << "Error finding key name for: " << key_handle;
    return return_code;
  }
  TPM2B_DIGEST tpm_digest = Make_TPM2B_DIGEST(digest);
  TPMT_SIGNATURE signature_out;
  TPMT_TK_HASHCHECK validation;
  validation.tag = TPM_ST_HASHCHECK;
  validation.digest.size = 0;
  session_->SetEntityAuthorizationValue(password);
  return_code = factory_.GetTpm()->SignSync(key_handle,
                                            key_name,
                                            tpm_digest,
                                            in_scheme,
                                            validation,
                                            &signature_out,
                                            session_->GetDelegate());
  if (return_code) {
    LOG(ERROR) << "Error signing digest: " << GetErrorString(return_code);
    return return_code;
  }
  if (scheme == TPM_ALG_RSAPSS) {
    signature->resize(signature_out.signature.rsapss.sig.size);
    signature->assign(StringFrom_TPM2B_PUBLIC_KEY_RSA(
        signature_out.signature.rsapss.sig));
  } else {
    signature->resize(signature_out.signature.rsassa.sig.size);
    signature->assign(StringFrom_TPM2B_PUBLIC_KEY_RSA(
        signature_out.signature.rsassa.sig));
  }
  return TPM_RC_SUCCESS;
}

TPM_RC TpmUtilityImpl::Verify(TPM_HANDLE key_handle,
                              TPM_ALG_ID scheme,
                              TPM_ALG_ID hash_alg,
                              const std::string& digest,
                              const std::string& signature) {
  TPM2B_PUBLIC public_area;
  TPM_RC return_code = GetKeyPublicArea(key_handle, &public_area);
  if (return_code) {
    LOG(ERROR) << "Error finding public area for: " << key_handle;
    return return_code;
  } else if (public_area.public_area.type != TPM_ALG_RSA) {
    LOG(ERROR) << "Key handle given is not an RSA key";
    return SAPI_RC_BAD_PARAMETER;
  } else if ((public_area.public_area.object_attributes & kSign) == 0) {
    LOG(ERROR) << "Key handle given is not a signing key";
    return SAPI_RC_BAD_PARAMETER;
  } else if ((public_area.public_area.object_attributes & kRestricted) != 0) {
    LOG(ERROR) << "Cannot use RSAPSS for signing with a restricted key";
    return SAPI_RC_BAD_PARAMETER;
  }
  TPMT_SIGNATURE signature_in;
  TPM_ALG_ID hash_in;
  if (hash_alg == TPM_ALG_NULL) {
    hash_in = TPM_ALG_SHA256;
  } else {
    hash_in = hash_alg;
  }
  if (scheme == TPM_ALG_RSAPSS) {
    signature_in.sig_alg = TPM_ALG_RSAPSS;
    signature_in.signature.rsapss.hash = hash_in;
    signature_in.signature.rsapss.sig = Make_TPM2B_PUBLIC_KEY_RSA(signature);
  } else if (scheme == TPM_ALG_NULL || scheme == TPM_ALG_RSASSA) {
    signature_in.sig_alg = TPM_ALG_RSASSA;
    signature_in.signature.rsassa.hash = hash_in;
    signature_in.signature.rsassa.sig = Make_TPM2B_PUBLIC_KEY_RSA(signature);
  } else {
    LOG(ERROR) << "Invalid scheme used to verify signature.";
    return SAPI_RC_BAD_PARAMETER;
  }
  NullAuthorizationDelegate null_delegate;
  std::string key_name;
  TPMT_TK_VERIFIED verified;
  TPM2B_DIGEST tpm_digest = Make_TPM2B_DIGEST(digest);
  return_code = factory_.GetTpm()->VerifySignatureSync(key_handle,
                                                       key_name,
                                                       tpm_digest,
                                                       signature_in,
                                                       &verified,
                                                       &null_delegate);
  if (return_code == TPM_RC_SIGNATURE) {
    LOG(WARNING) << "Incorrect signature for given digest.";
    return TPM_RC_SIGNATURE;
  } else if (return_code && return_code != TPM_RC_SIGNATURE) {
    LOG(ERROR) << "Error verifying signature: " << GetErrorString(return_code);
    return return_code;
  }
  return TPM_RC_SUCCESS;
}

TPM_RC TpmUtilityImpl::InitializeSession() {
  if (session_.get()) {
    return TPM_RC_SUCCESS;
  }
  scoped_ptr<AuthorizationSession> tmp_session(
      factory_.GetAuthorizationSession());
  TPM_RC result = tmp_session->StartUnboundSession(true /*Enable encryption.*/);
  if (result) {
    return result;
  }
  session_ = tmp_session.Pass();
  return TPM_RC_SUCCESS;
}

TPM_RC TpmUtilityImpl::SetHierarchyAuthorization(
    TPMI_RH_HIERARCHY_AUTH hierarchy,
    const std::string& password,
    AuthorizationDelegate* authorization) {
  return factory_.GetTpm()->HierarchyChangeAuthSync(
      hierarchy,
      NameFromHandle(hierarchy),
      Make_TPM2B_DIGEST(crypto::SHA256HashString(password)),
      authorization);
}

TPM_RC TpmUtilityImpl::DisablePlatformHierarchy(
    AuthorizationDelegate* authorization) {
  return factory_.GetTpm()->HierarchyControlSync(
      TPM_RH_PLATFORM,  // The authorizing entity.
      NameFromHandle(TPM_RH_PLATFORM),
      TPM_RH_PLATFORM,  // The target hierarchy.
      0,  // Disable.
      authorization);
}

TPM_RC TpmUtilityImpl::GetKeyName(TPM_HANDLE handle, std::string* name) {
  NullAuthorizationDelegate null_delegate;
  TPM2B_PUBLIC public_data;
  TPM2B_NAME out_name;
  out_name.size = 0;
  TPM2B_NAME qualified_name;
  std::string handle_name;  // Unused
  TPM_RC return_code = factory_.GetTpm()->ReadPublicSync(handle,
                                                         handle_name,
                                                         &public_data,
                                                         &out_name,
                                                         &qualified_name,
                                                         &null_delegate);
  if (return_code) {
    LOG(ERROR) << "Error generating name for object: " << handle;
    return return_code;
  }
  name->resize(out_name.size);
  name->assign(StringFrom_TPM2B_NAME(out_name));
  return TPM_RC_SUCCESS;
}

TPM_RC TpmUtilityImpl::GetKeyPublicArea(TPM_HANDLE handle,
                                        TPM2B_PUBLIC* public_data) {
  CHECK(public_data);
  NullAuthorizationDelegate null_delegate;
  TPM2B_NAME out_name;
  TPM2B_NAME qualified_name;
  std::string handle_name;  // Unused
  TPM_RC return_code = factory_.GetTpm()->ReadPublicSync(handle,
                                                         handle_name,
                                                         public_data,
                                                         &out_name,
                                                         &qualified_name,
                                                         &null_delegate);
  if (return_code) {
    LOG(ERROR) << "Error generating name for object: " << handle;
    return return_code;
  }
  return TPM_RC_SUCCESS;
}

}  // namespace trunks
