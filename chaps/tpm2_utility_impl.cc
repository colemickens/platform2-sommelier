// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chaps/tpm2_utility_impl.h"

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/macros.h>
#include <base/sha1.h>
#include <base/stl_util.h>
#include <crypto/scoped_openssl_types.h>
#include <openssl/rsa.h>

#include "trunks/error_codes.h"
#include "trunks/tpm_generated.h"
#include "trunks/tpm_state.h"
#include "trunks/trunks_factory_impl.h"

using chromeos::SecureBlob;
using std::map;
using std::set;
using trunks::kRSAStorageRootKey;
using trunks::TPM_RC;
using trunks::TPM_RC_SUCCESS;
using trunks::TrunksFactory;

namespace {

uint32_t GetIntegerExponent(const std::string& public_exponent) {
  uint32_t exponent = 0;
  for (size_t i = 0; i < public_exponent.size(); i++) {
    exponent = exponent << 8;
    exponent += public_exponent[i];
  }
  return exponent;
}

}  // namespace

namespace chaps {

TPM2UtilityImpl::TPM2UtilityImpl()
    : default_factory_(new trunks::TrunksFactoryImpl()),
      factory_(default_factory_.get()),
      is_initialized_(false),
      is_enabled_ready_(false),
      is_enabled_(false),
      session_(factory_->GetHmacSession()),
      trunks_tpm_utility_(factory_->GetTpmUtility()) {}

TPM2UtilityImpl::TPM2UtilityImpl(TrunksFactory* factory)
    : factory_(factory),
      is_initialized_(false),
      is_enabled_ready_(false),
      is_enabled_(false),
      session_(factory_->GetHmacSession()),
      trunks_tpm_utility_(factory_->GetTpmUtility()) {}

TPM2UtilityImpl::~TPM2UtilityImpl() {
  for (const auto& it : slot_handles_) {
    set<int> slot_handles = it.second;
    for (const auto& it2 : slot_handles) {
      if (factory_->GetTpm()->FlushContextSync(it2, NULL) != TPM_RC_SUCCESS) {
        LOG(WARNING) << "Error flushing handle: " << it2;
      }
    }
  }
}

bool TPM2UtilityImpl::Init() {
  scoped_ptr<trunks::TpmState> tpm_state = factory_->GetTpmState();
  TPM_RC result;
  result = tpm_state->Initialize();
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error getting TPM state information: "
               << trunks::GetErrorString(result);
    return false;
  }
  // Check if firmware initialized the platform hierarchy.
  if (tpm_state->IsPlatformHierarchyEnabled()) {
    LOG(ERROR) << "Platform initialization not complete.";
    return false;
  }
  // Check if ownership is taken. If not, TPMUtility initialization fails.
  if (!tpm_state->IsOwnerPasswordSet() ||
      !tpm_state->IsEndorsementPasswordSet() ||
      !tpm_state->IsLockoutPasswordSet()) {
    LOG(ERROR) << "TPM2Utility cannot be ready if the TPM is not owned.";
    return false;
  }
  result = session_->StartUnboundSession(true /* enable encryption */);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error starting an AuthorizationSession: "
               << trunks::GetErrorString(result);
    return false;
  }
  is_initialized_ = true;
  return true;
}

bool TPM2UtilityImpl::IsTPMAvailable() {
  if (is_enabled_ready_) {
    return is_enabled_;
  }
  // If the TPM works, it is available.
  if (is_initialized_) {
    is_enabled_ready_ = true;
    is_enabled_ = true;
    return true;
  }
  scoped_ptr<trunks::TpmState> tpm_state = factory_->GetTpmState();
  TPM_RC result = tpm_state->Initialize();
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error getting TPM state information: "
               << trunks::GetErrorString(result);
    return false;
  }
  is_enabled_ = tpm_state->IsEnabled();
  is_enabled_ready_ = true;
  return is_enabled_;
}

bool TPM2UtilityImpl::Authenticate(int slot_id,
                                   const SecureBlob& auth_data,
                                   const std::string& auth_key_blob,
                                   const std::string& encrypted_master_key,
                                   SecureBlob* master_key) {
  CHECK(master_key);
  int key_handle = 0;
  if (!LoadKey(slot_id, auth_key_blob, auth_data, &key_handle)) {
    return false;
  }
  std::string master_key_str;
  if (!Unbind(key_handle, encrypted_master_key, &master_key_str)) {
    return false;
  }
  *master_key = SecureBlob(master_key_str);
  master_key_str.clear();
  return true;
}

bool TPM2UtilityImpl::ChangeAuthData(int slot_id,
                                     const SecureBlob& old_auth_data,
                                     const SecureBlob& new_auth_data,
                                     const std::string& old_auth_key_blob,
                                     std::string* new_auth_key_blob) {
  int key_handle;
  if (new_auth_data.size() > SHA256_DIGEST_SIZE) {
    LOG(ERROR) << "Authorization cannot be larger than SHA256 Digest size.";
    return false;
  }
  if (!LoadKey(slot_id, old_auth_key_blob, old_auth_data, &key_handle)) {
    LOG(ERROR) << "Error loading key under old authorization data.";
    return false;
  }
  session_->SetEntityAuthorizationValue(old_auth_data.to_string());
  TPM_RC result = trunks_tpm_utility_->ChangeKeyAuthorizationData(
      key_handle,
      new_auth_data.to_string(),
      session_->GetDelegate(),
      new_auth_key_blob);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error changing authorization data: "
               << trunks::GetErrorString(result);
    return false;
  }
  result = factory_->GetTpm()->FlushContextSync(key_handle, NULL);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error unloading key under old authorization: "
               << trunks::GetErrorString(result);
    return false;
  }
  return true;
}

bool TPM2UtilityImpl::GenerateRandom(int num_bytes, std::string* random_data) {
  TPM_RC result = trunks_tpm_utility_->GenerateRandom(num_bytes,
                                                      nullptr,
                                                      random_data);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error generating random data from the TPM: "
               << trunks::GetErrorString(result);
    return false;
  }
  return true;
}

bool TPM2UtilityImpl::StirRandom(const std::string& entropy_data) {
  TPM_RC result = trunks_tpm_utility_->StirRandom(entropy_data, nullptr);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error seeding TPM random number generator: "
               << trunks::GetErrorString(result);
    return false;
  }
  return true;
}

bool TPM2UtilityImpl::GenerateKey(int slot,
                                  int modulus_bits,
                                  const std::string& public_exponent,
                                  const SecureBlob& auth_data,
                                  std::string* key_blob,
                                  int* key_handle) {
  if (public_exponent.size() > 4) {
    LOG(ERROR) << "Incorrectly formatted public_exponent.";
    return false;
  }
  if (auth_data.size() > SHA256_DIGEST_SIZE) {
    LOG(ERROR) << "Authorization cannot be larger than SHA256 Digest size.";
    return false;
  }
  if (modulus_bits < static_cast<int>(kMinModulusSize)) {
    LOG(ERROR) << "Minimum modulus size is: " << kMinModulusSize;
  }
  session_->SetEntityAuthorizationValue("");  // SRK Authorization Value.
  TPM_RC result = trunks_tpm_utility_->CreateRSAKeyPair(
      trunks::TpmUtility::AsymmetricKeyUsage::kDecryptAndSignKey,
      modulus_bits,
      GetIntegerExponent(public_exponent),
      auth_data.to_string(),
      "",  // Policy Digest
      false,  // use_only_policy_authorization
      trunks::kNoCreationPCR,
      session_->GetDelegate(),
      key_blob,
      nullptr);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error creating RSA key pair: "
               << trunks::GetErrorString(result);
    return false;
  }
  if (!LoadKey(slot, *key_blob, auth_data, key_handle)) {
    return false;
  }
  return true;
}

bool TPM2UtilityImpl::GetPublicKey(int key_handle,
                                   std::string* public_exponent,
                                   std::string* modulus) {
  trunks::TPMT_PUBLIC public_data;
  TPM_RC result = trunks_tpm_utility_->GetKeyPublicArea(key_handle,
                                                        &public_data);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error getting key public data: " << result;
    return false;
  }
  public_exponent->clear();
  result = trunks::Serialize_UINT32(public_data.parameters.rsa_detail.exponent,
                                    public_exponent);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error serializing public exponent: " << result;
    return false;
  }
  modulus->assign(StringFrom_TPM2B_PUBLIC_KEY_RSA(public_data.unique.rsa));
  return true;
}

bool TPM2UtilityImpl::WrapKey(int slot,
                              const std::string& public_exponent,
                              const std::string& modulus,
                              const std::string& prime_factor,
                              const SecureBlob& auth_data,
                              std::string* key_blob,
                              int* key_handle) {
  if (public_exponent.size() > 4) {
    LOG(ERROR) << "Incorrectly formatted public_exponent.";
    return false;
  }
  if (auth_data.size() > SHA256_DIGEST_SIZE) {
    LOG(ERROR) << "Authorization cannot be larger than SHA256 Digest size.";
    return false;
  }
  if (modulus.size() < kMinModulusSize) {
    LOG(ERROR) << "Minimum modulus size is: " << kMinModulusSize;
    return false;
  }
  session_->SetEntityAuthorizationValue("");  // SRK Authorization Value.
  TPM_RC result = trunks_tpm_utility_->ImportRSAKey(
      trunks::TpmUtility::AsymmetricKeyUsage::kDecryptAndSignKey,
      modulus,
      GetIntegerExponent(public_exponent),
      prime_factor,
      auth_data.to_string(),
      session_->GetDelegate(),
      key_blob);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error importing RSA key to TPM: "
               << trunks::GetErrorString(result);
    return false;
  }
  if (!LoadKey(slot, *key_blob, auth_data, key_handle)) {
    return false;
  }
  return true;
}

bool TPM2UtilityImpl::LoadKey(int slot,
                              const std::string& key_blob,
                              const SecureBlob& auth_data,
                              int* key_handle) {
  return LoadKeyWithParent(slot,
                           key_blob,
                           auth_data,
                           kRSAStorageRootKey,
                           key_handle);
}

bool TPM2UtilityImpl::LoadKeyWithParent(int slot,
                                        const std::string& key_blob,
                                        const SecureBlob& auth_data,
                                        int parent_key_handle,
                                        int* key_handle) {
  CHECK_EQ(parent_key_handle, static_cast<int>(kRSAStorageRootKey))
      << "Chaps with TPM2.0 only loads keys under the RSA SRK.";
  if (auth_data.size() > SHA256_DIGEST_SIZE) {
    LOG(ERROR) << "Authorization cannot be larger than SHA256 Digest size.";
    return false;
  }
  session_->SetEntityAuthorizationValue("");  // SRK Authorization Value.
  TPM_RC result = trunks_tpm_utility_->LoadKey(
      key_blob, session_->GetDelegate(),
      reinterpret_cast<trunks::TPM_HANDLE*>(key_handle));
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error loading key into TPM: "
               << trunks::GetErrorString(result);
    return false;
  }
  std::string key_name;
  result = trunks_tpm_utility_->GetKeyName(*key_handle, &key_name);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error getting key name: " << trunks::GetErrorString(result);
    return false;
  }
  handle_auth_data_[*key_handle] = auth_data;
  handle_name_[*key_handle] = key_name;
  slot_handles_[slot].insert(*key_handle);
  return true;
}

void TPM2UtilityImpl::UnloadKeysForSlot(int slot) {
  for (const auto& it : slot_handles_[slot]) {
    if (factory_->GetTpm()->FlushContextSync(it, NULL) != TPM_RC_SUCCESS) {
      LOG(WARNING) << "Error flushing handle: " << it;
    }
  }
  slot_handles_.erase(slot);
}

bool TPM2UtilityImpl::Bind(int key_handle,
                           const std::string& input,
                           std::string* output) {
  CHECK(output);
  // Max input size is the size of smallest allowed modulus - 11.
  uint32_t max_input_size = kMinModulusSize - 11;
  if (input.size() > max_input_size) {
    LOG(ERROR) << "Encryption plaintext is longer than RSA modulus.";
    return false;
  }
  crypto::ScopedRSA rsa(RSA_new());
  std::string modulus;
  std::string exponent;
  if (!GetPublicKey(key_handle, &exponent, &modulus)) {
    return false;
  }
  rsa.get()->n = BN_bin2bn(
      reinterpret_cast<const unsigned char*>(modulus.data()),
      modulus.size(),
      nullptr);
  rsa.get()->e = BN_bin2bn(
      reinterpret_cast<const unsigned char*>(exponent.data()),
      exponent.size(),
      nullptr);
  // RSA encrypt output should be size of the modulus.
  output->resize(modulus.size());
  int rsa_result = RSA_public_encrypt(
      input.size(),
      reinterpret_cast<const unsigned char*>(input.data()),
      reinterpret_cast<unsigned char*>(string_as_array(output)),
      rsa.get(),
      RSA_PKCS1_PADDING);
  if (rsa_result == -1) {
    LOG(ERROR) << "Error performing RSA_public_encrypt.";
    return false;
  }
  return true;
}

bool TPM2UtilityImpl::Unbind(int key_handle,
                             const std::string& input,
                             std::string* output) {
  // Max input size is the size of smallest allowed modulus.
  if (input.size() > kMinModulusSize) {
    LOG(ERROR) << "RSA decrypt ciphertext is larger than modulus.";
    return false;
  }
  std::string auth_data = handle_auth_data_[key_handle].to_string();
  session_->SetEntityAuthorizationValue(auth_data);
  TPM_RC result = trunks_tpm_utility_->AsymmetricDecrypt(key_handle,
                                              trunks::TPM_ALG_RSAES,
                                              trunks::TPM_ALG_SHA1,
                                              input,
                                              session_->GetDelegate(),
                                              output);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error performing unbind operation: "
               << trunks::GetErrorString(result);
    return false;
  }
  return true;
}

bool TPM2UtilityImpl::Sign(int key_handle,
                           const std::string& input,
                           std::string* signature) {
  std::string auth_data = handle_auth_data_[key_handle].to_string();
  session_->SetEntityAuthorizationValue(auth_data);
  TPM_RC result = trunks_tpm_utility_->Sign(key_handle,
                                 trunks::TPM_ALG_RSASSA,
                                 trunks::TPM_ALG_SHA1,
                                 input,
                                 session_->GetDelegate(),
                                 signature);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error performing sign operation: "
               << trunks::GetErrorString(result);
    return false;
  }
  return true;
}

bool TPM2UtilityImpl::Verify(int key_handle,
                             const std::string& input,
                             const std::string& signature) {
  std::string digest = base::SHA1HashString(input);
  crypto::ScopedRSA rsa(RSA_new());
  std::string modulus;
  std::string exponent;
  if (!GetPublicKey(key_handle, &exponent, &modulus)) {
    return false;
  }
  rsa.get()->n = BN_bin2bn(
      reinterpret_cast<const unsigned char*>(modulus.data()),
      modulus.size(),
      nullptr);
  rsa.get()->e = BN_bin2bn(
      reinterpret_cast<const unsigned char*>(exponent.data()),
      exponent.size(),
      nullptr);
  if (RSA_verify(NID_sha1,
                 reinterpret_cast<const unsigned char*>(digest.data()),
                 digest.size(),
                 reinterpret_cast<const unsigned char*>(signature.data()),
                 signature.size(),
                 rsa.get()) != 1) {
    LOG(ERROR) << "Signature was incorrect.";
    return false;
  }
  return true;
}

bool TPM2UtilityImpl::IsSRKReady() {
  return IsTPMAvailable() && Init();
}

}  // namespace chaps
