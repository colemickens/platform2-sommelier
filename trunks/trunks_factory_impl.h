// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TRUNKS_TRUNKS_FACTORY_IMPL_H_
#define TRUNKS_TRUNKS_FACTORY_IMPL_H_

#include "trunks/trunks_factory.h"

#include <string>

#include <base/macros.h>
#include <base/memory/scoped_ptr.h>

#include "trunks/command_transceiver.h"
#include "trunks/trunks_export.h"

namespace trunks {

class Tpm;
class TrunksProxy;

// TrunksFactoryImpl is the default TrunksFactory implementation.
class TRUNKS_EXPORT TrunksFactoryImpl : public TrunksFactory {
 public:
  // Uses TrunksProxy as the default CommandTransceiver to pass to the TPM.
  TrunksFactoryImpl();
  // TrunksFactoryImpl does not take ownership of |transceiver|. This
  // transceiver is forwarded down to the Tpm instance maintained by
  // this factory.
  explicit TrunksFactoryImpl(CommandTransceiver* transceiver);
  ~TrunksFactoryImpl() override;

  // TrunksFactory methods.
  Tpm* GetTpm() const override;
  scoped_ptr<TpmState> GetTpmState() const override;
  scoped_ptr<TpmUtility> GetTpmUtility() const override;
  scoped_ptr<AuthorizationDelegate> GetPasswordAuthorization(
      const std::string& password) const override;
  scoped_ptr<SessionManager> GetSessionManager() const override;
  scoped_ptr<HmacSession> GetHmacSession() const override;
  scoped_ptr<PolicySession> GetPolicySession() const override;
  scoped_ptr<PolicySession> GetTrialSession() const override;
  scoped_ptr<BlobParser> GetBlobParser() const override;

 private:
  scoped_ptr<CommandTransceiver> default_transceiver_;
  CommandTransceiver* transceiver_;
  scoped_ptr<Tpm> tpm_;

  DISALLOW_COPY_AND_ASSIGN(TrunksFactoryImpl);
};

}  // namespace trunks

#endif  // TRUNKS_TRUNKS_FACTORY_IMPL_H_
