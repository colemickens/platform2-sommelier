// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "trunks/trunks_factory_impl.h"

#include "trunks/authorization_session.h"
#include "trunks/hmac_authorization_session.h"
#include "trunks/password_authorization_delegate.h"
#include "trunks/tpm_generated.h"
#include "trunks/tpm_state_impl.h"
#include "trunks/tpm_utility_impl.h"
#include "trunks/trunks_proxy.h"

namespace trunks {

TrunksFactoryImpl::TrunksFactoryImpl(Tpm* tpm) : tpm_(tpm) {}

TrunksFactoryImpl::TrunksFactoryImpl() : proxy_(new TrunksProxy()),
                                         default_tpm_(new Tpm(proxy_.get())),
                                         tpm_(default_tpm_.get()) {
  if (!proxy_->Init()) {
    LOG(ERROR) << "Failed to initialize trunks proxy.";
  }
}

TrunksFactoryImpl::~TrunksFactoryImpl() {
}

Tpm* TrunksFactoryImpl::GetTpm() const {
  return tpm_;
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

scoped_ptr<AuthorizationSession>
    TrunksFactoryImpl::GetHmacAuthorizationSession() const {
  return scoped_ptr<AuthorizationSession>(new HmacAuthorizationSession(*this));
}

}  // namespace trunks
