// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "trunks/policy_session_impl.h"

#include <string>

#include <base/logging.h>
#include <base/macros.h>
#include <base/stl_util.h>
#include <openssl/rand.h>


#include "trunks/tpm_generated.h"

namespace trunks {

PolicySessionImpl::PolicySessionImpl(const TrunksFactory& factory)
    : factory_(factory) {
  session_manager_ = factory_.GetSessionManager();
}

PolicySessionImpl::~PolicySessionImpl() {
  session_manager_->CloseSession();
}

AuthorizationDelegate* PolicySessionImpl::GetDelegate() {
  if (session_manager_->GetSessionHandle() == kUninitializedHandle) {
    return NULL;
  }
  return &hmac_delegate_;
}

TPM_RC PolicySessionImpl::StartBoundSession(
    TPMI_DH_ENTITY bind_entity,
    const std::string& bind_authorization_value,
    bool enable_encryption) {
  return session_manager_->StartSession(TPM_SE_POLICY, bind_entity,
                                        bind_authorization_value,
                                        enable_encryption, &hmac_delegate_);
}

TPM_RC PolicySessionImpl::StartUnboundSession(
    bool enable_encryption) {
  // Just like a HmacAuthorizationSession, an unbound policy session is just
  // a session bound to TPM_RH_NULL.
  return StartBoundSession(TPM_RH_NULL, "", enable_encryption);
}

}  // namespace trunks
