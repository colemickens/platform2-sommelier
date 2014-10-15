// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TRUNKS_NULL_AUTHORIZATION_DELEGATE_H_
#define TRUNKS_NULL_AUTHORIZATION_DELEGATE_H_

#include <string>

#include <base/macros.h>
#include <chromeos/chromeos_export.h>

#include "trunks/authorization_delegate.h"

namespace trunks {

// NullAuthorizationDelegate is an implementation of the AuthorizationDelegate
// interface. This delegate is used when authorization and parameter encryption
// is not enabled. It returns no authorization data, performs no checks and
// does no parameter encryption.
class CHROMEOS_EXPORT NullAuthorizationDelegate: public AuthorizationDelegate {
 public:
  NullAuthorizationDelegate();
  virtual ~NullAuthorizationDelegate();
  // AuthorizationDelegate methods.
  virtual bool GetCommandAuthorization(const std::string& command_hash,
                                       std::string* authorization);
  virtual bool CheckResponseAuthorization(const std::string& response_hash,
                                          const std::string& authorization);
  virtual bool EncryptCommandParameter(std::string* parameter);
  virtual bool DecryptResponseParameter(std::string* parameter);

 private:
  DISALLOW_COPY_AND_ASSIGN(NullAuthorizationDelegate);
};

}  // namespace trunks

#endif  // TRUNKS_NULL_AUTHORIZATION_DELEGATE_H_
