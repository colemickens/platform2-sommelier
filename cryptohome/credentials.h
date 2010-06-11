// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Credentials is the interface for objects that wrap up a set
// of credentials with which we can authenticate or mount.

#ifndef CRYPTOHOME_CREDENTIALS_H_
#define CRYPTOHOME_CREDENTIALS_H_

#include "secure_blob.h"

namespace cryptohome {

class Credentials {
 public:
  Credentials() {}
  virtual ~Credentials() {}

  // Returns the full user name, including any '@' sign or domain name.
  //
  // Parameters
  //  name_buffer - Output buffer.
  //  length - Amount of space in name_buffer
  //
  virtual void GetFullUsername(char *name_buffer, int length) const = 0;

  // Returns the full user name as a std::string
  //
  // Parameters
  //
  virtual std::string GetFullUsernameString() const = 0;

  // Returns the part of the username before the '@'
  //
  // Parameters
  //  name_buffer - Output buffer.
  //  length - Amount of space in name_buffer
  //
  virtual void GetPartialUsername(char *name_buffer, int length) const = 0;

  // Returns the obfuscated username, used as the name of the directory
  // containing the user's stateful data (and maybe used for other reasons
  // at some point.)
  virtual std::string GetObfuscatedUsername(
      const chromeos::Blob &system_salt) const = 0;

  // Returns a the user's passkey
  //
  // Parameters
  //  passkey - A SecureBlob containing the passkey
  //
  virtual void GetPasskey(SecureBlob* passkey) const = 0;
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_CREDENTIALS_H_
