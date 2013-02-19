// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pkcs11_keystore.h"

#include <string>

#include <chromeos/secure_blob.h>

using chromeos::SecureBlob;
using std::string;

namespace cryptohome {

// TODO(dkrahn): implement this class
Pkcs11KeyStore::Pkcs11KeyStore() {}

Pkcs11KeyStore::~Pkcs11KeyStore() {}

bool Pkcs11KeyStore::Read(const string& key_name,
                          SecureBlob* key_data) {
  return false;
}

bool Pkcs11KeyStore::Write(const string& key_name,
                           const SecureBlob& key_data) {
  return false;
}

}  // namespace cryptohome
