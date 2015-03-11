// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TRUNKS_TRUNKS_FACTORY_H_
#define TRUNKS_TRUNKS_FACTORY_H_

#include <string>

#include <base/macros.h>
#include <base/memory/scoped_ptr.h>

#include "trunks/trunks_export.h"

namespace trunks {

class AuthorizationDelegate;
class AuthorizationSession;
class Tpm;
class TpmState;
class TpmUtility;

// TrunksFactory is an interface to act as a factory for trunks objects. This
// mechanism assists in injecting mocks for testing. This class is not
// thread-safe.
class TRUNKS_EXPORT TrunksFactory {
 public:
  TrunksFactory() {}
  virtual ~TrunksFactory() {}

  // Returns a Tpm instance. The caller does not take ownership.
  virtual Tpm* GetTpm() const = 0;

  // Returns an uninitialized TpmState instance. The caller takes ownership.
  virtual scoped_ptr<TpmState> GetTpmState() const = 0;

  // Returns a TpmUtility instance. The caller takes ownership.
  virtual scoped_ptr<TpmUtility> GetTpmUtility() const = 0;

  // Returns an AuthorizationDelegate instance for basic password authorization.
  // The caller takes ownership.
  virtual scoped_ptr<AuthorizationDelegate> GetPasswordAuthorization(
      const std::string& password) const = 0;

  // Returns an AuthorizationSession instance. The caller takes ownership.
  virtual scoped_ptr<AuthorizationSession> GetAuthorizationSession() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(TrunksFactory);
};

}  // namespace trunks

#endif  // TRUNKS_TRUNKS_FACTORY_H_
