// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_OBFUSCATED_USERNAME_H_
#define CRYPTOHOME_OBFUSCATED_USERNAME_H_

#include <string>

#include <brillo/secure_blob.h>

namespace cryptohome {

// Returns the obfuscated username, used as the name of the directory containing
// the user's stateful data (and maybe used for other reasons at some point.)
// The |username| must be non-empty.
std::string BuildObfuscatedUsername(const std::string& username,
                                    const brillo::SecureBlob& system_salt);

}  // namespace cryptohome

#endif  // CRYPTOHOME_OBFUSCATED_USERNAME_H_
