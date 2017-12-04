// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBPASSWORDPROVIDER_PASSWORD_PROVIDER_H_
#define LIBPASSWORDPROVIDER_PASSWORD_PROVIDER_H_

#include <memory>

#include <brillo/brillo_export.h>

#include "libpasswordprovider/password.h"

namespace password_provider {

// Implementation of password storage. This is a wrapper around Linux keyring
// functions.

// Saves the given password to the keyring of the calling process. The password
// will be available to be retrieved until the process that calls SavePassword
// dies.
bool BRILLO_EXPORT SavePassword(const Password& password);

// Retrieves the given password. The returned password will be null terminated.
// Calling GetPassword after DiscardPassword has been called by any process will
// return false.
std::unique_ptr<Password> BRILLO_EXPORT GetPassword();

// Discards the saved password.
bool BRILLO_EXPORT DiscardPassword();

}  // namespace password_provider

#endif  // LIBPASSWORDPROVIDER_PASSWORD_PROVIDER_H_
