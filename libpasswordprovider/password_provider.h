// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBPASSWORDPROVIDER_PASSWORD_PROVIDER_H_
#define LIBPASSWORDPROVIDER_PASSWORD_PROVIDER_H_

#include <memory>

#include <base/macros.h>
#include <brillo/brillo_export.h>

#include "libpasswordprovider/password.h"

namespace password_provider {

class BRILLO_EXPORT PasswordProviderInterface {
 public:
  virtual ~PasswordProviderInterface() {}

  // Saves the given password to the keyring of the calling process.
  // The password will be available to be retrieved until the process that calls
  // SavePassword dies.
  virtual bool SavePassword(const Password& password) = 0;

  // Retrieves the given password. The returned password will be null
  // terminated. Calling GetPassword after DiscardPassword has been called by
  // any process will return false.
  virtual std::unique_ptr<Password> GetPassword() = 0;

  // Discards the saved password.
  virtual bool DiscardPassword() = 0;
};

// Implementation of password storage. This is a wrapper around Linux keyring
// functions.
class BRILLO_EXPORT PasswordProvider : public PasswordProviderInterface {
 public:
  PasswordProvider();

  // PasswordProviderInterface overrides
  bool BRILLO_EXPORT SavePassword(const Password& password) override;
  std::unique_ptr<Password> BRILLO_EXPORT GetPassword() override;
  bool BRILLO_EXPORT DiscardPassword() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(PasswordProvider);
};

}  // namespace password_provider

#endif  // LIBPASSWORDPROVIDER_PASSWORD_PROVIDER_H_
