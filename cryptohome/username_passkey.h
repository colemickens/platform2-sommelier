// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// UsernamePasskey wraps a username/passkey pair that can be used to
// authenticate a user.

#ifndef CRYPTOHOME_USERNAME_PASSKEY_H_
#define CRYPTOHOME_USERNAME_PASSKEY_H_

#include "cryptohome/credentials.h"

#include <string.h>

#include "base/basictypes.h"

namespace cryptohome {

class UsernamePasskey : public Credentials {
 public:
  // Constructs UsernamePasskey from username strings and passkeys
  UsernamePasskey(const char *username, const int username_length,
                   const char *passkey, const int passkey_length);
  UsernamePasskey(const char *username, const int username_length,
                   const chromeos::Blob& passkey);
  UsernamePasskey(const char *username, const int username_length,
                   const std::string& passkey);
  UsernamePasskey(const UsernamePasskey& rhs);

  ~UsernamePasskey();

  UsernamePasskey& operator=(const UsernamePasskey& rhs);

  // Constructs UsernamePasskey from a username and password, converting the
  // password to a passkey using the given salt
  static UsernamePasskey FromUsernamePassword(const char* username,
                                              const char* password,
                                              const chromeos::Blob& salt);

  // Overridden from cryptohome::Credentials
  void GetFullUsername(char *name_buffer, int length) const;
  std::string GetFullUsername() const;
  void GetPartialUsername(char *name_buffer, int length) const;
  std::string GetObfuscatedUsername(const chromeos::Blob &system_salt) const;
  SecureBlob GetPasskey() const;

 private:
  static SecureBlob PasswordToPasskey(const char *password,
                                      const chromeos::Blob& salt);
  static void AsciiEncodeToBuffer(const unsigned char* source,
                                  int source_length, char* buffer,
                                  int buffer_length);

  std::string username_;
  SecureBlob passkey_;
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_USERNAME_PASSKEY_H_
