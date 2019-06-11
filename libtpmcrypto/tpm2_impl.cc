// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libtpmcrypto/tpm2_impl.h"

#include <base/logging.h>
#include <base/threading/platform_thread.h>
#include <brillo/secure_blob.h>
#include <trunks/error_codes.h>

using brillo::Blob;
using brillo::SecureBlob;
using trunks::AuthorizationDelegate;
using trunks::GetErrorString;
using trunks::HmacSession;
using trunks::PolicySession;
using trunks::TPM_RC;
using trunks::TPM_RC_SUCCESS;
using trunks::TPMS_NV_PUBLIC;
using trunks::TrunksFactoryImpl;

namespace tpmcrypto {

using PcrMap = std::map<uint32_t, std::string>;

Tpm2Impl::Tpm2Impl() = default;
Tpm2Impl::~Tpm2Impl() = default;

std::unique_ptr<Tpm> CreateTpmInstance() {
  return std::make_unique<Tpm2Impl>();
}

bool Tpm2Impl::SealToPCR0(const SecureBlob& value, SecureBlob* sealed_value) {
  CHECK(sealed_value);

  std::string policy_digest;
  std::unique_ptr<HmacSession> session;
  return EnsureInitialized() && CreatePcr0PolicyDigest(&policy_digest) &&
         CreateHmacSession(&session) &&
         SealData(session->GetDelegate(), policy_digest, value, sealed_value);
}

bool Tpm2Impl::Unseal(const SecureBlob& sealed_value, SecureBlob* value) {
  CHECK(value);

  std::unique_ptr<PolicySession> policy_session;
  return EnsureInitialized() && CreatePolicySessionForPCR0(&policy_session) &&
         UnsealData(policy_session->GetDelegate(), sealed_value, value);
}

bool Tpm2Impl::EnsureInitialized() {
  if (is_initialized_) {
    return true;
  }

  // Create and initialize trunks factory.
  factory_impl_ = std::make_unique<TrunksFactoryImpl>();
  if (!factory_impl_->Initialize()) {
    LOG(ERROR) << "Failed to initialize trunks factory.";
    factory_impl_.reset();
    return false;
  }

  // Create TPM utility using trunks factory.
  tpm_utility_ = factory_impl_->GetTpmUtility();
  if (!tpm_utility_) {
    return false;
  }

  is_initialized_ = true;
  return true;
}

bool Tpm2Impl::CreatePcr0PolicyDigest(std::string* policy_digest) {
  TPM_RC result = tpm_utility_->GetPolicyDigestForPcrValues(
      PcrMap({{0, ""}}), false /* use_auth_value */, policy_digest);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error getting policy digest: " << GetErrorString(result);
    return false;
  }

  return true;
}

bool Tpm2Impl::CreateHmacSession(std::unique_ptr<HmacSession>* hmac_session) {
  std::unique_ptr<HmacSession> session = factory_impl_->GetHmacSession();
  if (tpm_utility_->StartSession(session.get()) != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error starting hmac session.";
    return false;
  }

  hmac_session->swap(session);
  return true;
}

bool Tpm2Impl::CreatePolicySessionForPCR0(
    std::unique_ptr<PolicySession>* policy_session) {
  std::unique_ptr<PolicySession> session = factory_impl_->GetPolicySession();
  TPM_RC result = session->StartUnboundSession(false /* salted */,
                                               false /* enable_encryption */);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error starting policy session: " << GetErrorString(result);
    return false;
  }

  result = session->PolicyPCR(PcrMap({{0, ""}}));
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error restricting policy to pcr 0: "
               << GetErrorString(result);
    return false;
  }

  policy_session->swap(session);
  return true;
}

bool Tpm2Impl::SealData(AuthorizationDelegate* session_delegate,
                        const std::string& policy_digest,
                        const SecureBlob& value,
                        SecureBlob* sealed_value) {
  const std::string data_to_seal(value.begin(), value.end());
  std::string sealed_data_str;
  TPM_RC result = tpm_utility_->SealData(data_to_seal, policy_digest, "",
                                         session_delegate, &sealed_data_str);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error sealing data: " << GetErrorString(result);
    return false;
  }

  sealed_value->assign(sealed_data_str.begin(), sealed_data_str.end());
  return true;
}

bool Tpm2Impl::UnsealData(AuthorizationDelegate* policy_delegate,
                          const SecureBlob& sealed_value,
                          SecureBlob* value) {
  const std::string sealed_data(sealed_value.begin(), sealed_value.end());
  std::string unsealed_data;
  TPM_RC result =
      tpm_utility_->UnsealData(sealed_data, policy_delegate, &unsealed_data);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error unsealing data: " << GetErrorString(result);
    return false;
  }

  value->assign(unsealed_data.begin(), unsealed_data.end());
  return true;
}

bool Tpm2Impl::GetNVAttributes(uint32_t index, uint32_t* attributes) {
  CHECK(attributes);
  if (!EnsureInitialized()) {
    return false;
  }
  TPMS_NV_PUBLIC space_info;
  TPM_RC result = tpm_utility_->GetNVSpacePublicArea(index, &space_info);
  if (result != trunks::TPM_RC_SUCCESS) {
    PLOG(ERROR) << "Failed to get the NVRAM space attributes";
    return false;
  }
  *attributes = space_info.attributes;
  return true;
}

bool Tpm2Impl::NVReadNoAuth(uint32_t index,
                            uint32_t offset,
                            size_t size,
                            std::string* data) {
  CHECK(data);
  if (!EnsureInitialized()) {
    return false;
  }
  const std::string kWellKnownPassword = "";
  auto pw_auth = factory_impl_->GetPasswordAuthorization(kWellKnownPassword);

  trunks::TPM_RC result = tpm_utility_->ReadNVSpace(
      index, offset, size, false /* using_owner_authorization */, data,
      pw_auth.get());
  if (result != trunks::TPM_RC_SUCCESS) {
    PLOG(ERROR) << "Failed to read TPM space index: " << index;
    return false;
  }
  return true;
}

}  // namespace tpmcrypto
