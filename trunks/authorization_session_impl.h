// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TRUNKS_AUTHORIZATION_SESSION_IMPL_H_
#define TRUNKS_AUTHORIZATION_SESSION_IMPL_H_

#include "trunks/authorization_session.h"

#include <string>

#include <base/macros.h>
#include <chromeos/chromeos_export.h>

#include "trunks/hmac_authorization_delegate.h"
#include "trunks/password_authorization_delegate.h"
#include "trunks/trunks_factory.h"

namespace trunks {

/*
 * This class implements the AuthorizationSession interface. It is used for
 * keeping track of the HmacAuthorizationDelegate used for commands, and to
 * provide authorization for commands that need it. It is instantiated by
 * TpmUtilityImpl. If we need to use this class outside of TpmUtility, we
 * can use it as below:
 * TrunksFactoryImpl factory;
 * AuthorizationSessionImpl session(factory);
 * session.StartBoundSession(bind_entity, bind_authorization, true);
 * session.SetEntityAuthorizationValue(entity_authorization);
 * factory.GetTpm()->RSA_EncrpytSync(_,_,_,_, session.GetDelegate());
 *
 * NOTE: StartBoundSession/StartUnboundSession should not be called before
 * TPM Ownership is taken. This is because starting a session uses the
 * SaltingKey, which is only created after ownership is taken.
 */
class CHROMEOS_EXPORT AuthorizationSessionImpl: public AuthorizationSession {
 public:
  // The constructor for AuthroizationSessionImpl needs a factory. In
  // producation code, this factory is used to access the TPM class to forward
  // commands to the TPM. In test code, this is used to mock out the TPM calls.
  explicit AuthorizationSessionImpl(const TrunksFactory& factory);
  virtual ~AuthorizationSessionImpl();

  // AuthorizationSession methods.
  AuthorizationDelegate* GetDelegate() override;
  TPM_RC StartBoundSession(TPMI_DH_ENTITY bind_entity,
                           const std::string& bind_authorization_value,
                           bool enable_encryption) override;
  TPM_RC StartUnboundSession(bool enable_encryption) override;
  void SetEntityAuthorizationValue(const std::string& value) override;
  void SetFutureAuthorizationValue(const std::string& value) override;

 private:
  // This function is used to encrypt a plaintext salt |salt|, using RSA
  // public encrypt with the SaltingKey PKCS1_OAEP padding. It follows the
  // specification defined in TPM2.0 Part 1 Architecture, Appendix B.10.2.
  // The encrypted salt is stored in the out parameter |encrypted_salt|.
  TPM_RC EncryptSalt(const std::string& salt, std::string* encrypted_salt);

  // This method tries to flush all TPM context associated with the current
  // AuthorizationSession.
  void CloseSession();

  // This factory is only set in the constructor and is used to instantiate
  // The TPM class to forward commands to the TPM chip.
  const TrunksFactory& factory_;
  // This delegate is what provides authorization to commands. It is what is
  // returned when the GetDelegate method is called.
  HmacAuthorizationDelegate hmac_delegate_;
  // This handle keeps track of the TPM session. It is issued by the TPM,
  // and is only modified when a new TPM session is started using
  // StartBoundSession or StartUnboundSession. We use this to keep track of
  // the session handle, so that we can clean it up when this class is
  // destroyed.
  TPM_HANDLE hmac_handle_;

  friend class AuthorizationSessionTest;
  DISALLOW_COPY_AND_ASSIGN(AuthorizationSessionImpl);
};

}  // namespace trunks

#endif  // TRUNKS_AUTHORIZATION_SESSION_IMPL_H_
