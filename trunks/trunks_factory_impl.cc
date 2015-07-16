// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "trunks/trunks_factory_impl.h"

#include "trunks/blob_parser.h"
#include "trunks/hmac_session_impl.h"
#include "trunks/password_authorization_delegate.h"
#include "trunks/policy_session_impl.h"
#include "trunks/session_manager_impl.h"
#include "trunks/tpm_generated.h"
#include "trunks/tpm_state_impl.h"
#include "trunks/tpm_utility_impl.h"
#include "trunks/trunks_proxy.h"

namespace trunks {

TrunksFactoryImpl::TrunksFactoryImpl() {
  default_transceiver_.reset(new TrunksProxy());
  transceiver_ = default_transceiver_.get();
  tpm_.reset(new Tpm(transceiver_));
  if (!transceiver_->Init()) {
    LOG(ERROR) << "Error initializing transceiver.";
  }
}

TrunksFactoryImpl::TrunksFactoryImpl(CommandTransceiver* transceiver) {
  transceiver_ = transceiver;
  tpm_.reset(new Tpm(transceiver_));
}

TrunksFactoryImpl::~TrunksFactoryImpl() {}

Tpm* TrunksFactoryImpl::GetTpm() const {
  return tpm_.get();
}

scoped_ptr<TpmState> TrunksFactoryImpl::GetTpmState() const {
  return scoped_ptr<TpmState>(new TpmStateImpl(*this));
}

scoped_ptr<TpmUtility> TrunksFactoryImpl::GetTpmUtility() const {
  return scoped_ptr<TpmUtility>(new TpmUtilityImpl(*this));
}

scoped_ptr<AuthorizationDelegate> TrunksFactoryImpl::GetPasswordAuthorization(
    const std::string& password) const {
  return scoped_ptr<AuthorizationDelegate>(
      new PasswordAuthorizationDelegate(password));
}

scoped_ptr<SessionManager> TrunksFactoryImpl::GetSessionManager() const {
  return scoped_ptr<SessionManager>(new SessionManagerImpl(*this));
}

scoped_ptr<HmacSession> TrunksFactoryImpl::GetHmacSession() const {
  return scoped_ptr<HmacSession>(new HmacSessionImpl(*this));
}

scoped_ptr<PolicySession> TrunksFactoryImpl::GetPolicySession() const {
  return scoped_ptr<PolicySession>(new PolicySessionImpl(*this, TPM_SE_POLICY));
}

scoped_ptr<PolicySession> TrunksFactoryImpl::GetTrialSession() const {
  return scoped_ptr<PolicySession>(new PolicySessionImpl(*this, TPM_SE_TRIAL));
}

scoped_ptr<BlobParser> TrunksFactoryImpl::GetBlobParser() const {
  return scoped_ptr<BlobParser>(new BlobParser());
}

}  // namespace trunks
