// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A PKCS #11 backed KeyStore implementation.

#ifndef CRYPTOHOME_PKCS11_KEYSTORE_H_
#define CRYPTOHOME_PKCS11_KEYSTORE_H_

#include "keystore.h"

#include <string>

#include <chromeos/secure_blob.h>

namespace cryptohome {

class Pkcs11KeyStore : public KeyStore {
 public:
  Pkcs11KeyStore();
  virtual ~Pkcs11KeyStore();

  // KeyStore interface.
  virtual bool Read(const std::string& key_name,
                    chromeos::SecureBlob* key_data);
  virtual bool Write(const std::string& key_name,
                     const chromeos::SecureBlob& key_data);

 private:
  DISALLOW_COPY_AND_ASSIGN(Pkcs11KeyStore);
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_PKCS11_KEYSTORE_H_
