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
class BlobParser;
class HmacSession;
class PolicySession;
class SessionManager;
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

  // Returns a SessionManager instance. The caller takes ownership.
  virtual scoped_ptr<SessionManager> GetSessionManager() const = 0;

  // Returns a HmacSession instance. The caller takes ownership.
  virtual scoped_ptr<HmacSession> GetHmacSession() const = 0;

  // Returns a PolicySession instance. The caller takes ownership.
  virtual scoped_ptr<PolicySession> GetPolicySession() const = 0;

  // Returns a TrialSession instance. The caller takes ownership.
  virtual scoped_ptr<PolicySession> GetTrialSession() const = 0;

  // Returns a BlobParser instance. The caller takes ownership.
  virtual scoped_ptr<BlobParser> GetBlobParser() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(TrunksFactory);
};

}  // namespace trunks

#endif  // TRUNKS_TRUNKS_FACTORY_H_
