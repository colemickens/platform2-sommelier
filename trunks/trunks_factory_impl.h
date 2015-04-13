// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TRUNKS_TRUNKS_FACTORY_IMPL_H_
#define TRUNKS_TRUNKS_FACTORY_IMPL_H_

#include "trunks/trunks_factory.h"

#include <string>

#include <base/macros.h>
#include <base/memory/scoped_ptr.h>

#include "trunks/trunks_export.h"

namespace trunks {

class Tpm;
class TrunksProxy;

// TrunksFactoryImpl is the default TrunksFactory implementation.
class TRUNKS_EXPORT TrunksFactoryImpl : public TrunksFactory {
 public:
  // All objects created by this factory will share |tpm|. This class does not
  // take ownership of the pointer, it must remain valid for the lifetime of the
  // factory.
  explicit TrunksFactoryImpl(Tpm* tpm);
  // A default Tpm instance will be used which sends commands to a TrunksProxy.
  TrunksFactoryImpl();
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

 private:
  scoped_ptr<TrunksProxy> proxy_;
  scoped_ptr<Tpm> default_tpm_;
  Tpm* tpm_;

  DISALLOW_COPY_AND_ASSIGN(TrunksFactoryImpl);
};

}  // namespace trunks

#endif  // TRUNKS_TRUNKS_FACTORY_IMPL_H_
