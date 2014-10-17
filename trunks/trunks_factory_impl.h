// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TRUNKS_TRUNKS_FACTORY_IMPL_H_
#define TRUNKS_TRUNKS_FACTORY_IMPL_H_

#include "trunks/trunks_factory.h"

#include <string>

#include <base/macros.h>
#include <base/memory/scoped_ptr.h>
#include <chromeos/chromeos_export.h>

namespace trunks {

class Tpm;
class TrunksProxy;

// TrunksFactoryImpl is the default TrunksFactory implementation.
class CHROMEOS_EXPORT TrunksFactoryImpl : public TrunksFactory {
 public:
  TrunksFactoryImpl();
  virtual ~TrunksFactoryImpl();

  // TrunksFactory methods.
  Tpm* GetTpm() const override;
  scoped_ptr<TpmState> GetTpmState() const override;
  scoped_ptr<TpmUtility> GetTpmUtility() const override;
  scoped_ptr<AuthorizationDelegate> GetPasswordAuthorization(
      const std::string& password) const override;
  scoped_ptr<AuthorizationSession> GetAuthorizationSession() const override;

 private:
  scoped_ptr<TrunksProxy> proxy_;
  scoped_ptr<Tpm> tpm_;

  DISALLOW_COPY_AND_ASSIGN(TrunksFactoryImpl);
};

}  // namespace trunks

#endif  // TRUNKS_TRUNKS_FACTORY_IMPL_H_
