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
  virtual bool Read(const std::string& key_name,
                    chromeos::SecureBlob* key_data) = 0;

  // Writes key data to the store for the key identified by |key_name|.  If such
  // a key already exists the existing data will be overwritten.
  virtual bool Write(const std::string& key_name,
                     const chromeos::SecureBlob& key_data) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(KeyStore);
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_KEYSTORE_H_
