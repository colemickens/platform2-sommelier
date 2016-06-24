// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains the implementation of class Tpm

#include "cryptohome/tpm2_impl.h"

#include <string>

#include <base/logging.h>
#include <base/memory/ptr_util.h>
#include <crypto/scoped_openssl_types.h>
#include <openssl/rsa.h>
#include <trunks/authorization_delegate.h>
#include <trunks/blob_parser.h>
#include <trunks/error_codes.h>
#include <trunks/policy_session.h>
#include <trunks/tpm_constants.h>
#include <trunks/tpm_generated.h>
#include <trunks/trunks_factory.h>
#include <trunks/trunks_factory_impl.h>
#include <trunks/trunks_dbus_proxy.h>

#include "cryptohome/cryptolib.h"

using brillo::SecureBlob;
using trunks::GetErrorString;
using trunks::TPM_RC;
using trunks::TPM_RC_SUCCESS;
using trunks::TrunksFactory;

namespace cryptohome {

Tpm2Impl::Tpm2Impl(TrunksFactory* factory)
    : has_external_trunks_context_(true) {
  external_trunks_context_.factory = factory;
  external_trunks_context_.tpm_state = factory->GetTpmState();
  external_trunks_context_.tpm_utility = factory->GetTpmUtility();
}

bool Tpm2Impl::GetOwnerPassword(brillo::Blob* owner_password) {
  if (!owner_password_.empty()) {
    owner_password->assign(owner_password_.begin(), owner_password_.end());
  } else {
    owner_password->clear();
  }
  return true;
}

void Tpm2Impl::SetOwnerPassword(const SecureBlob& owner_password) {
  SecureBlob tmp(owner_password);
  owner_password_.swap(tmp);
}

bool Tpm2Impl::PerformEnabledOwnedCheck(bool* enabled, bool* owned) {
  TrunksClientContext* trunks;
  if (!GetTrunksContext(&trunks)) {
    return false;
  }
  TPM_RC result = trunks->tpm_state->Initialize();
  if (result != TPM_RC_SUCCESS) {
    if (enabled) {
      *enabled = false;
    }
    if (owned) {
      *owned = false;
    }
    LOG(ERROR) << "Error getting tpm status for owned check: "
               << GetErrorString(result);
    return false;
  }
  if (enabled) {
    *enabled = trunks->tpm_state->IsEnabled();
  }
  if (owned) {
    *owned = trunks->tpm_state->IsOwned();
  }
  return true;
}

bool Tpm2Impl::GetRandomData(size_t length, brillo::Blob* data) {
  CHECK(data);
  TrunksClientContext* trunks;
  if (!GetTrunksContext(&trunks)) {
    return false;
  }
  std::string random_data;
  TPM_RC result =
      trunks->tpm_utility->GenerateRandom(length,
                                          nullptr,  // No authorization.
                                          &random_data);
  if ((result != TPM_RC_SUCCESS) || (random_data.size() != length)) {
    LOG(ERROR) << "Error getting random data: " << GetErrorString(result);
    return false;
  }
  data->assign(random_data.begin(), random_data.end());
  return true;
}

bool Tpm2Impl::DefineNvram(uint32_t index, size_t length, uint32_t flags) {
  TrunksClientContext* trunks;
  if (!GetTrunksContext(&trunks)) {
    return false;
  }
  if (flags != (Tpm::kTpmNvramWriteDefine | Tpm::kTpmNvramBindToPCR0)) {
    LOG(ERROR) << "Defining NVram with unsupported flags.";
    return false;
  }
  trunks::TPMA_NV attributes =
      trunks::TPMA_NV_OWNERWRITE | trunks::TPMA_NV_WRITEDEFINE |
      trunks::TPMA_NV_POLICYREAD | trunks::TPMA_NV_NO_DA;
  if (owner_password_.empty()) {
    LOG(ERROR) << "Defining NVram needs owner_password.";
    return false;
  }
  std::unique_ptr<trunks::HmacSession> session =
      trunks->factory->GetHmacSession();
  TPM_RC result = session->StartUnboundSession(true /* Enable encryption */);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error starting a session: " << GetErrorString(result);
    return false;
  }
  session->SetEntityAuthorizationValue(owner_password_.to_string());
  std::string policy;
  result = trunks->tpm_utility->GetPolicyDigestForPcrValue(
      0 /* PCR_0 */, std::string() /* Use the current PCR value */, &policy);
  result = trunks->tpm_utility->DefineNVSpace(
      index, length, attributes, std::string() /* No authorization value */,
      policy, session->GetDelegate());
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error defining NVram space: " << GetErrorString(result);
    return false;
  }
  return true;
}

bool Tpm2Impl::DestroyNvram(uint32_t index) {
  TrunksClientContext* trunks;
  if (!GetTrunksContext(&trunks)) {
    return false;
  }
  if (owner_password_.empty()) {
    LOG(ERROR) << "Destroying NVram needs owner_password.";
    return false;
  }
  std::unique_ptr<trunks::HmacSession> session =
      trunks->factory->GetHmacSession();
  TPM_RC result = session->StartUnboundSession(true /* Enable encryption */);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error starting a session: " << GetErrorString(result);
    return false;
  }
  session->SetEntityAuthorizationValue(owner_password_.to_string());
  result = trunks->tpm_utility->DestroyNVSpace(index, session->GetDelegate());
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error destroying NVram space: " << GetErrorString(result);
    return false;
  }
  return true;
}

bool Tpm2Impl::WriteNvram(uint32_t index, const SecureBlob& blob) {
  TrunksClientContext* trunks;
  if (!GetTrunksContext(&trunks)) {
    return false;
  }
  if (owner_password_.empty()) {
    LOG(ERROR) << "Writing NVram needs owner_password.";
    return false;
  }
  std::unique_ptr<trunks::HmacSession> session =
      trunks->factory->GetHmacSession();
  TPM_RC result = session->StartUnboundSession(true /* Enable encryption */);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error starting a session: " << GetErrorString(result);
    return false;
  }
  session->SetEntityAuthorizationValue(owner_password_.to_string());
  result = trunks->tpm_utility->WriteNVSpace(
      index, 0, /* offset */
      blob.to_string(), true /* using_owner_authorization */,
      false /* extend */, session->GetDelegate());
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error writing to NVSpace: " << GetErrorString(result);
    return false;
  }
  result = trunks->tpm_utility->LockNVSpace(
      index, false /* lock_read */, true /* lock_write */,
      true /* using_owner_authorization */, session->GetDelegate());
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error locking NVSpace: " << GetErrorString(result);
    return false;
  }
  return true;
}

bool Tpm2Impl::ReadNvram(uint32_t index, SecureBlob* blob) {
  TrunksClientContext* trunks;
  if (!GetTrunksContext(&trunks)) {
    return false;
  }
  std::unique_ptr<trunks::PolicySession> session =
      trunks->factory->GetPolicySession();
  TPM_RC result = session->StartUnboundSession(true /* Enable encryption */);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error starting a session: " << GetErrorString(result);
    return false;
  }
  result = session->PolicyPCR(0 /* PCR_0 */,
                              std::string() /* Use current PCR value */);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error setting up policy session: " << GetErrorString(result);
    return false;
  }
  std::string nvram_data;
  result = trunks->tpm_utility->ReadNVSpace(
      index, 0 /* offset */, GetNvramSize(index),
      false /* using_owner_authorization */, &nvram_data,
      session->GetDelegate());
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error reading from nvram: " << GetErrorString(result);
    return false;
  }
  blob->assign(nvram_data.begin(), nvram_data.end());
  return true;
}

bool Tpm2Impl::IsNvramDefined(uint32_t index) {
  TrunksClientContext* trunks;
  if (!GetTrunksContext(&trunks)) {
    return false;
  }
  trunks::TPMS_NV_PUBLIC nvram_public;
  TPM_RC result =
      trunks->tpm_utility->GetNVSpacePublicArea(index, &nvram_public);
  if (trunks::GetFormatOneError(result) == trunks::TPM_RC_HANDLE) {
    return false;
  } else if (result == TPM_RC_SUCCESS) {
    return true;
  } else {
    LOG(ERROR) << "Error reading NV space for index " << index
               << " with error: " << GetErrorString(result);
  }
  return false;
}

bool Tpm2Impl::IsNvramLocked(uint32_t index) {
  TrunksClientContext* trunks;
  if (!GetTrunksContext(&trunks)) {
    return false;
  }
  trunks::TPMS_NV_PUBLIC nvram_public;
  TPM_RC result =
      trunks->tpm_utility->GetNVSpacePublicArea(index, &nvram_public);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error reading NV space for index " << index
               << " with error: " << GetErrorString(result);
    return false;
  }
  return (nvram_public.attributes & trunks::TPMA_NV_WRITELOCKED) != 0;
}

unsigned int Tpm2Impl::GetNvramSize(uint32_t index) {
  TrunksClientContext* trunks;
  if (!GetTrunksContext(&trunks)) {
    return false;
  }
  trunks::TPMS_NV_PUBLIC nvram_public;
  TPM_RC result =
      trunks->tpm_utility->GetNVSpacePublicArea(index, &nvram_public);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error reading NV space for index " << index
               << " with error: " << GetErrorString(result);
    return 0;
  }
  return nvram_public.data_size;
}

bool Tpm2Impl::IsEndorsementKeyAvailable() {
  LOG(ERROR) << __func__ << ": Not Implemented.";
  return false;
}

bool Tpm2Impl::CreateEndorsementKey() {
  LOG(ERROR) << __func__ << ": Not Implemented.";
  return false;
}

bool Tpm2Impl::GetEndorsementPublicKey(SecureBlob* ek_public_key) {
  LOG(ERROR) << __func__ << ": Not Implemented.";
  return false;
}

bool Tpm2Impl::GetEndorsementCredential(SecureBlob* credential) {
  LOG(ERROR) << __func__ << ": Not Implemented.";
  return false;
}

bool Tpm2Impl::MakeIdentity(SecureBlob* identity_public_key_der,
                            SecureBlob* identity_public_key,
                            SecureBlob* identity_key_blob,
                            SecureBlob* identity_binding,
                            SecureBlob* identity_label,
                            SecureBlob* pca_public_key,
                            SecureBlob* endorsement_credential,
                            SecureBlob* platform_credential,
                            SecureBlob* conformance_credential) {
  LOG(ERROR) << __func__ << ": Not Implemented.";
  return false;
}

bool Tpm2Impl::QuotePCR(int pcr_index,
                        const SecureBlob& identity_key_blob,
                        const SecureBlob& external_data,
                        SecureBlob* pcr_value,
                        SecureBlob* quoted_data,
                        SecureBlob* quote) {
  LOG(ERROR) << __func__ << ": Not Implemented.";
  return false;
}

bool Tpm2Impl::SealToPCR0(const brillo::Blob& value,
                          brillo::Blob* sealed_value) {
  TrunksClientContext* trunks;
  if (!GetTrunksContext(&trunks)) {
    return false;
  }
  std::string policy_digest;
  TPM_RC result =
      trunks->tpm_utility->GetPolicyDigestForPcrValue(0, "", &policy_digest);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error getting policy digest: " << GetErrorString(result);
    return false;
  }
  std::unique_ptr<trunks::HmacSession> session =
      trunks->factory->GetHmacSession();
  if (trunks->tpm_utility->StartSession(session.get()) != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error starting hmac session.";
    return false;
  }
  std::string data_to_seal(value.begin(), value.end());
  std::string sealed_data;
  result = trunks->tpm_utility->SealData(data_to_seal, policy_digest,
                                         session->GetDelegate(), &sealed_data);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error creating sealed object: " << GetErrorString(result);
    return false;
  }
  sealed_value->assign(sealed_data.begin(), sealed_data.end());
  return true;
}

bool Tpm2Impl::Unseal(const brillo::Blob& sealed_value, brillo::Blob* value) {
  TrunksClientContext* trunks;
  if (!GetTrunksContext(&trunks)) {
    return false;
  }
  std::unique_ptr<trunks::PolicySession> policy_session =
      trunks->factory->GetPolicySession();
  TPM_RC result = policy_session->StartUnboundSession(false);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error starting policy session: " << GetErrorString(result);
    return false;
  }
  result = policy_session->PolicyPCR(0, "");
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error restricting policy to pcr 0: "
               << GetErrorString(result);
    return false;
  }
  std::string sealed_data(sealed_value.begin(), sealed_value.end());
  std::string unsealed_data;
  result = trunks->tpm_utility->UnsealData(
      sealed_data, policy_session->GetDelegate(), &unsealed_data);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error unsealing object: " << GetErrorString(result);
    return false;
  }
  value->assign(unsealed_data.begin(), unsealed_data.end());
  return true;
}

bool Tpm2Impl::CreateCertifiedKey(const SecureBlob& identity_key_blob,
                                  const SecureBlob& external_data,
                                  SecureBlob* certified_public_key,
                                  SecureBlob* certified_public_key_der,
                                  SecureBlob* certified_key_blob,
                                  SecureBlob* certified_key_info,
                                  SecureBlob* certified_key_proof) {
  LOG(ERROR) << __func__ << ": Not Implemented.";
  return false;
}

bool Tpm2Impl::CreateDelegate(const SecureBlob& identity_key_blob,
                              SecureBlob* delegate_blob,
                              SecureBlob* delegate_secret) {
  LOG(ERROR) << __func__ << ": Not Implemented.";
  return false;
}

bool Tpm2Impl::ActivateIdentity(const SecureBlob& delegate_blob,
                                const SecureBlob& delegate_secret,
                                const SecureBlob& identity_key_blob,
                                const SecureBlob& encrypted_asym_ca,
                                const SecureBlob& encrypted_sym_ca,
                                SecureBlob* identity_credential) {
  LOG(ERROR) << __func__ << ": Not Implemented.";
  return false;
}

bool Tpm2Impl::TakeOwnership(int max_timeout_tries,
                             const SecureBlob& owner_password) {
  LOG(ERROR) << __func__ << ": Not Implemented.";
  return false;
}

bool Tpm2Impl::InitializeSrk(const SecureBlob& owner_password) {
  LOG(ERROR) << __func__ << ": Not Implemented.";
  return false;
}

bool Tpm2Impl::ChangeOwnerPassword(const SecureBlob& previous_owner_password,
                                   const SecureBlob& owner_password) {
  LOG(ERROR) << __func__ << ": Not Implemented.";
  return false;
}

bool Tpm2Impl::TestTpmAuth(const SecureBlob& owner_password) {
  LOG(ERROR) << __func__ << ": Not Implemented.";
  return false;
}

bool Tpm2Impl::IsTransient(TpmRetryAction retry_action) {
  return false;
}

bool Tpm2Impl::Sign(const SecureBlob& key_blob,
                    const SecureBlob& input,
                    int bound_pcr_index,
                    SecureBlob* signature) {
  TrunksClientContext* trunks;
  if (!GetTrunksContext(&trunks)) {
    return false;
  }
  trunks::AuthorizationDelegate* delegate;
  std::unique_ptr<trunks::PolicySession> policy_session;
  std::unique_ptr<trunks::HmacSession> hmac_session;
  TPM_RC result;
  if (bound_pcr_index >= 0) {
    policy_session = trunks->factory->GetPolicySession();
    result = policy_session->StartUnboundSession(false);
    if (result != TPM_RC_SUCCESS) {
      LOG(ERROR) << "Error starting policy session: " << GetErrorString(result);
      return false;
    }
    result = policy_session->PolicyPCR(bound_pcr_index, "");
    if (result != TPM_RC_SUCCESS) {
      LOG(ERROR) << "Error creating PCR policy: " << GetErrorString(result);
      return false;
    }
    delegate = policy_session->GetDelegate();
  } else {
    hmac_session = trunks->factory->GetHmacSession();
    result = hmac_session->StartUnboundSession(true);
    if (result != TPM_RC_SUCCESS) {
      LOG(ERROR) << "Error starting hmac session: " << GetErrorString(result);
      return false;
    }
    hmac_session->SetEntityAuthorizationValue("");
    delegate = hmac_session->GetDelegate();
  }

  ScopedKeyHandle handle;
  TpmRetryAction retry = LoadWrappedKey(key_blob, &handle);
  if (retry != kTpmRetryNone) {
    LOG(ERROR) << "Error loading pcr bound key.";
    return false;
  }
  std::string tpm_signature;
  result = trunks->tpm_utility->Sign(handle.value(), trunks::TPM_ALG_RSASSA,
                                     trunks::TPM_ALG_SHA256, input.to_string(),
                                     delegate, &tpm_signature);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error signing: " << GetErrorString(result);
    return false;
  }
  signature->assign(tpm_signature.begin(), tpm_signature.end());
  return true;
}

bool Tpm2Impl::CreatePCRBoundKey(int pcr_index,
                                 const SecureBlob& pcr_value,
                                 SecureBlob* key_blob,
                                 SecureBlob* public_key_der,
                                 SecureBlob* creation_blob) {
  CHECK(key_blob) << "No key blob argument provided.";
  CHECK(creation_blob) << "No creation blob argument provided.";
  TrunksClientContext* trunks;
  if (!GetTrunksContext(&trunks)) {
    return false;
  }
  std::string policy_digest;
  TPM_RC result = trunks->tpm_utility->GetPolicyDigestForPcrValue(
      pcr_index, pcr_value.to_string(), &policy_digest);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error getting policy digest: " << GetErrorString(result);
    return false;
  }
  std::string tpm_key_blob;
  std::string tpm_creation_blob;
  std::unique_ptr<trunks::AuthorizationDelegate> delegate =
      trunks->factory->GetPasswordAuthorization("");
  result = trunks->tpm_utility->CreateRSAKeyPair(
      trunks::TpmUtility::kSignKey, kDefaultTpmRsaModulusSize,
      kDefaultTpmPublicExponent,
      "",  // No authorization
      policy_digest,
      true,  // use_only_policy_authorization
      pcr_index, delegate.get(), &tpm_key_blob,
      &tpm_creation_blob /* No creation_blob */);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error creating a pcr bound key: " << GetErrorString(result);
    return false;
  }
  key_blob->assign(tpm_key_blob.begin(), tpm_key_blob.end());
  creation_blob->assign(tpm_creation_blob.begin(), tpm_creation_blob.end());

  // if |public_key_der| is present, create and assign it.
  if (public_key_der) {
    trunks::TPM2B_PUBLIC public_data;
    trunks::TPM2B_PRIVATE private_data;
    if (!trunks->factory->GetBlobParser()->ParseKeyBlob(
            key_blob->to_string(), &public_data, &private_data)) {
      return false;
    }
    if (!PublicAreaToPublicKeyDER(public_data.public_area, public_key_der)) {
      return false;
    }
  }
  return true;
}

bool Tpm2Impl::VerifyPCRBoundKey(int pcr_index,
                                 const SecureBlob& pcr_value,
                                 const SecureBlob& key_blob,
                                 const SecureBlob& creation_blob) {
  TrunksClientContext* trunks;
  if (!GetTrunksContext(&trunks)) {
    return false;
  }
  // First we verify that the PCR were in a known good state at the time of
  // Key creation.
  trunks::TPM2B_CREATION_DATA creation_data;
  trunks::TPM2B_DIGEST creation_hash;
  trunks::TPMT_TK_CREATION creation_ticket;
  if (!trunks->factory->GetBlobParser()->ParseCreationBlob(
          creation_blob.to_string(), &creation_data, &creation_hash,
          &creation_ticket)) {
    LOG(ERROR) << "Error parsing creation_blob.";
    return false;
  }
  trunks::TPML_PCR_SELECTION& pcr_select =
      creation_data.creation_data.pcr_select;
  if (pcr_select.count != 1) {
    LOG(ERROR) << "Creation data missing creation PCR value.";
    return false;
  }
  if (pcr_select.pcr_selections[0].hash != trunks::TPM_ALG_SHA256) {
    LOG(ERROR) << "Creation PCR extended with wrong hash algorithm.";
    return false;
  }
  if (pcr_select.pcr_selections[0].pcr_select[pcr_index / 8] !=
      (1 << (pcr_index % 8))) {
    LOG(ERROR) << "Incorrect creation PCR specified.";
    return false;
  }
  SecureBlob expected_pcr_digest = CryptoLib::Sha256(pcr_value);
  if (creation_data.creation_data.pcr_digest.size !=
      expected_pcr_digest.size()) {
    LOG(ERROR) << "Incorrect PCR digest size.";
    return false;
  }
  if (memcmp(creation_data.creation_data.pcr_digest.buffer,
             expected_pcr_digest.data(), expected_pcr_digest.size()) != 0) {
    LOG(ERROR) << "Incorrect PCR digest value.";
    return false;
  }
  // Then we certify that the key was created by the TPM.
  ScopedKeyHandle scoped_handle;
  if (LoadWrappedKey(key_blob, &scoped_handle) != Tpm::kTpmRetryNone) {
    return false;
  }
  TPM_RC result = trunks->tpm_utility->CertifyCreation(
      scoped_handle.value(), creation_blob.to_string());
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error certifying that key was created by TPM: "
               << trunks::GetErrorString(result);
    return false;
  }
  // Finally we verify that the key's policy_digest is the expected value.
  std::unique_ptr<trunks::PolicySession> trial_session =
      trunks->factory->GetTrialSession();
  result = trial_session->StartUnboundSession(true);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error starting a trial session: " << GetErrorString(result);
    return false;
  }
  result = trial_session->PolicyPCR(pcr_index, pcr_value.to_string());
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error restricting trial policy to pcr value: "
               << GetErrorString(result);
    return false;
  }
  std::string policy_digest;
  result = trial_session->GetDigest(&policy_digest);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error getting policy digest: " << GetErrorString(result);
    return false;
  }
  trunks::TPMT_PUBLIC public_area;
  result = trunks->tpm_utility->GetKeyPublicArea(scoped_handle.value(),
                                                 &public_area);
  if (result != TPM_RC_SUCCESS) {
    return false;
  }
  if (public_area.auth_policy.size != policy_digest.size()) {
    LOG(ERROR) << "Key auth policy and policy digest are of different length."
               << public_area.auth_policy.size << "," << policy_digest.size();
    return false;
  } else if (memcmp(public_area.auth_policy.buffer, policy_digest.data(),
                    policy_digest.size()) != 0) {
    LOG(ERROR) << "Key auth policy is different from policy digest.";
    return false;
  } else if (public_area.object_attributes & trunks::kUserWithAuth) {
    LOG(ERROR) << "Key authorization is not restricted to policy.";
    return false;
  }
  return true;
}

bool Tpm2Impl::ExtendPCR(int pcr_index, const SecureBlob& extension) {
  TrunksClientContext* trunks;
  if (!GetTrunksContext(&trunks)) {
    return false;
  }
  std::unique_ptr<trunks::AuthorizationDelegate> delegate =
      trunks->factory->GetPasswordAuthorization("");
  TPM_RC result = trunks->tpm_utility->ExtendPCR(
      pcr_index, extension.to_string(), delegate.get());
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error extending PCR: " << GetErrorString(result);
    return false;
  }
  return true;
}

bool Tpm2Impl::ReadPCR(int pcr_index, SecureBlob* pcr_value) {
  CHECK(pcr_value);
  TrunksClientContext* trunks;
  if (!GetTrunksContext(&trunks)) {
    return false;
  }
  std::string pcr_digest;
  TPM_RC result = trunks->tpm_utility->ReadPCR(pcr_index, &pcr_digest);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error reading from PCR: " << GetErrorString(result);
    return false;
  }
  pcr_value->assign(pcr_digest.begin(), pcr_digest.end());
  return true;
}

bool Tpm2Impl::WrapRsaKey(const SecureBlob& public_modulus,
                          const SecureBlob& prime_factor,
                          SecureBlob* wrapped_key) {
  TrunksClientContext* trunks;
  if (!GetTrunksContext(&trunks)) {
    return false;
  }
  std::string key_blob;
  std::unique_ptr<trunks::AuthorizationDelegate> delegate =
      trunks->factory->GetPasswordAuthorization("");
  TPM_RC result = trunks->tpm_utility->ImportRSAKey(
      trunks::TpmUtility::kDecryptKey, public_modulus.to_string(),
      kDefaultTpmPublicExponent, prime_factor.to_string(),
      "",  // No authorization,
      delegate.get(), &key_blob);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error creating SRK wrapped key: " << GetErrorString(result);
    return false;
  }
  wrapped_key->assign(key_blob.begin(), key_blob.end());
  return true;
}

Tpm::TpmRetryAction Tpm2Impl::LoadWrappedKey(const SecureBlob& wrapped_key,
                                             ScopedKeyHandle* key_handle) {
  CHECK(key_handle);
  TrunksClientContext* trunks;
  if (!GetTrunksContext(&trunks)) {
    return Tpm::kTpmRetryFailNoRetry;
  }
  trunks::TPM_HANDLE handle;
  std::unique_ptr<trunks::AuthorizationDelegate> delegate =
      trunks->factory->GetPasswordAuthorization("");
  TPM_RC result = trunks->tpm_utility->LoadKey(wrapped_key.to_string(),
                                               delegate.get(), &handle);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error loading SRK wrapped key: " << GetErrorString(result);
    return Tpm::kTpmRetryFailNoRetry;
  }
  key_handle->reset(this, handle);
  return Tpm::kTpmRetryNone;
}

bool Tpm2Impl::LegacyLoadCryptohomeKey(ScopedKeyHandle* key_handle,
                                       SecureBlob* key_blob) {
  // This doesn't apply to devices with TPM 2.0.
  return false;
}

void Tpm2Impl::CloseHandle(TpmKeyHandle key_handle) {
  TrunksClientContext* trunks;
  if (!GetTrunksContext(&trunks)) {
    return;
  }
  TPM_RC result =
      trunks->factory->GetTpm()->FlushContextSync(key_handle, nullptr);
  if (result != TPM_RC_SUCCESS) {
    LOG(WARNING) << "Error flushing tpm handle " << key_handle << ": "
                 << GetErrorString(result);
  }
}

Tpm::TpmRetryAction Tpm2Impl::EncryptBlob(TpmKeyHandle key_handle,
                                          const SecureBlob& plaintext,
                                          const SecureBlob& key,
                                          SecureBlob* ciphertext) {
  CHECK(ciphertext);
  TrunksClientContext* trunks;
  if (!GetTrunksContext(&trunks)) {
    return Tpm::kTpmRetryFailNoRetry;
  }
  std::string tpm_ciphertext;
  TPM_RC result = trunks->tpm_utility->AsymmetricEncrypt(
      key_handle, trunks::TPM_ALG_OAEP, trunks::TPM_ALG_SHA256,
      plaintext.to_string(), nullptr, &tpm_ciphertext);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error encrypting plaintext: " << GetErrorString(result);
    return Tpm::kTpmRetryFailNoRetry;
  }
  if (!CryptoLib::ObscureRSAMessage(SecureBlob(tpm_ciphertext), key,
                                    ciphertext)) {
    LOG(ERROR) << "Error obscuring tpm encrypted blob.";
    return Tpm::kTpmRetryFailNoRetry;
  }
  return Tpm::kTpmRetryNone;
}

Tpm::TpmRetryAction Tpm2Impl::DecryptBlob(TpmKeyHandle key_handle,
                                          const SecureBlob& ciphertext,
                                          const SecureBlob& key,
                                          SecureBlob* plaintext) {
  CHECK(plaintext);
  TrunksClientContext* trunks;
  if (!GetTrunksContext(&trunks)) {
    return Tpm::kTpmRetryFailNoRetry;
  }
  SecureBlob local_data;
  if (!CryptoLib::UnobscureRSAMessage(ciphertext, key, &local_data)) {
    LOG(ERROR) << "Error unobscureing message.";
    return Tpm::kTpmRetryFailNoRetry;
  }
  std::unique_ptr<trunks::AuthorizationDelegate> delegate =
      trunks->factory->GetPasswordAuthorization("");
  std::string tpm_plaintext;
  TPM_RC result = trunks->tpm_utility->AsymmetricDecrypt(
      key_handle, trunks::TPM_ALG_OAEP, trunks::TPM_ALG_SHA256,
      local_data.to_string(), delegate.get(), &tpm_plaintext);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error encrypting plaintext: " << GetErrorString(result);
    return Tpm::kTpmRetryFailNoRetry;
  }
  plaintext->assign(tpm_plaintext.begin(), tpm_plaintext.end());
  return Tpm::kTpmRetryNone;
}

Tpm::TpmRetryAction Tpm2Impl::GetPublicKeyHash(TpmKeyHandle key_handle,
                                               SecureBlob* hash) {
  CHECK(hash);
  TrunksClientContext* trunks;
  if (!GetTrunksContext(&trunks)) {
    return Tpm::kTpmRetryFailNoRetry;
  }
  trunks::TPMT_PUBLIC public_data;
  TPM_RC result =
      trunks->tpm_utility->GetKeyPublicArea(key_handle, &public_data);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error getting key public area: " << GetErrorString(result);
    return Tpm::kTpmRetryFailNoRetry;
  }
  std::string public_modulus =
      trunks::StringFrom_TPM2B_PUBLIC_KEY_RSA(public_data.unique.rsa);
  *hash = CryptoLib::Sha256(SecureBlob(public_modulus));
  return Tpm::kTpmRetryNone;
}

void Tpm2Impl::GetStatus(TpmKeyHandle key, TpmStatusInfo* status) {
  LOG(ERROR) << __func__ << ": Not Implemented.";
}

bool Tpm2Impl::GetDictionaryAttackInfo(int* counter,
                                       int* threshold,
                                       bool* lockout,
                                       int* seconds_remaining) {
  LOG(ERROR) << __func__ << ": Not Implemented.";
  return false;
}

bool Tpm2Impl::ResetDictionaryAttackMitigation(
    const SecureBlob& delegate_blob,
    const SecureBlob& delegate_secret) {
  LOG(ERROR) << __func__ << ": Not Implemented.";
  return false;
}

bool Tpm2Impl::GetTrunksContext(TrunksClientContext** trunks) {
  if (has_external_trunks_context_) {
    *trunks = &external_trunks_context_;
    return true;
  }
  base::PlatformThreadId thread_id = base::PlatformThread::CurrentId();
  if (trunks_contexts_.count(thread_id) == 0) {
    std::unique_ptr<TrunksClientContext> new_context(new TrunksClientContext);
    new_context->factory_impl = base::MakeUnique<trunks::TrunksFactoryImpl>();
    if (!new_context->factory_impl->Initialize()) {
      LOG(ERROR) << "Failed to initialize trunks factory.";
      return false;
    }
    new_context->factory = new_context->factory_impl.get();
    new_context->tpm_state = new_context->factory->GetTpmState();
    new_context->tpm_utility = new_context->factory->GetTpmUtility();
    trunks_contexts_[thread_id] = std::move(new_context);
  }
  *trunks = trunks_contexts_[thread_id].get();
  return true;
}

bool Tpm2Impl::PublicAreaToPublicKeyDER(const trunks::TPMT_PUBLIC& public_area,
                                        brillo::SecureBlob* public_key_der) {
  crypto::ScopedRSA rsa(RSA_new());
  rsa.get()->e = BN_new();
  CHECK(rsa.get()->e) << "Error setting exponent for RSA.";
  BN_set_word(rsa.get()->e, kDefaultTpmPublicExponent);
  rsa.get()->n = BN_bin2bn(public_area.unique.rsa.buffer,
                           public_area.unique.rsa.size, nullptr);
  CHECK(rsa.get()->n) << "Error setting modulus for RSA.";
  int der_length = i2d_RSAPublicKey(rsa.get(), nullptr);
  if (der_length < 0) {
    LOG(ERROR) << "Failed to get DER-encoded public key length.";
    return false;
  }
  public_key_der->resize(der_length);
  unsigned char* der_buffer = public_key_der->data();
  der_length = i2d_RSAPublicKey(rsa.get(), &der_buffer);
  if (der_length < 0) {
    LOG(ERROR) << "Failed to DER-encode public key.";
    return false;
  }
  return true;
}

}  // namespace cryptohome
