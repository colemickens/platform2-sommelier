// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A mock-able key storage interface.

#ifndef CRYPTOHOME_KEYSTORE_H_
#define CRYPTOHOME_KEYSTORE_H_

#include <string>

#include <chromeos/secure_blob.h>

namespace cryptohome {

class KeyStore {
 public:
  KeyStore() {}
  virtual ~KeyStore() {}

  // Reads key data from the store for the key identified by |key_name|.  On
  // success true is returned and |key_data| is populated.
  virtual bool Read(const std::string& username,
                    const std::string& key_name,
                    chromeos::SecureBlob* key_data) = 0;

  // Writes key data to the store for the key identified by |key_name|.  If such
  // a key already exists the existing data will be overwritten.
  virtual bool Write(const std::string& username,
                     const std::string& key_name,
                     const chromeos::SecureBlob& key_data) = 0;

  // Deletes key data for the key identified by |key_name|.  Returns false if
  // key data exists but could not be deleted.
  virtual bool Delete(const std::string& username,
                      const std::string& key_name) = 0;

  // Deletes key data for all keys identified by |key_prefix|.  Returns false if
  // key data exists but could not be deleted.
  virtual bool DeleteByPrefix(const std::string& username,
                              const std::string& key_prefix) = 0;

  // Registers a key.  |private_key_blob| holds the private key in some opaque
  // format and |public_key_der| holds the public key in PKCS #1 RSAPublicKey
  // format.  Returns true on success.
  virtual bool Register(const std::string& username,
                        const chromeos::SecureBlob& private_key_blob,
                        const chromeos::SecureBlob& public_key_der) = 0;
 private:
  DISALLOW_COPY_AND_ASSIGN(KeyStore);
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_KEYSTORE_H_
