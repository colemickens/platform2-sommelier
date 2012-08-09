// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/crypto_provider.h"

#include <base/memory/scoped_ptr.h>
#include <base/string_util.h>

#include "shill/crypto_des_cbc.h"
#include "shill/crypto_rot47.h"
#include "shill/logging.h"

using std::string;

namespace shill {

const char CryptoProvider::kKeyMatterFile[] = "/var/lib/whitelist/owner.key";

CryptoProvider::CryptoProvider(GLib *glib)
    : glib_(glib),
      key_matter_file_(kKeyMatterFile) {}

void CryptoProvider::Init() {
  cryptos_.reset();

  // Register the crypto modules in priority order -- highest priority first.
  scoped_ptr<CryptoDESCBC> des_cbc(new CryptoDESCBC(glib_));
  if (des_cbc->LoadKeyMatter(key_matter_file_)) {
    cryptos_.push_back(des_cbc.release());
  }
  cryptos_.push_back(new CryptoROT47());
}

string CryptoProvider::Encrypt(const string &plaintext) {
  for (Cryptos::iterator it = cryptos_.begin(); it != cryptos_.end(); ++it) {
    CryptoInterface *crypto = *it;
    string ciphertext;
    if (crypto->Encrypt(plaintext, &ciphertext)) {
      const string prefix = crypto->GetID() + ":";
      return prefix + ciphertext;
    }
  }
  LOG(WARNING) << "Unable to encrypt text, returning as is.";
  return plaintext;
}

string CryptoProvider::Decrypt(const string &ciphertext) {
  for (Cryptos::iterator it = cryptos_.begin(); it != cryptos_.end(); ++it) {
    CryptoInterface *crypto = *it;
    const string prefix = crypto->GetID() + ":";
    if (StartsWithASCII(ciphertext, prefix, true)) {
      string to_decrypt = ciphertext;
      to_decrypt.erase(0, prefix.size());
      string plaintext;
      if (!crypto->Decrypt(to_decrypt, &plaintext)) {
        LOG(WARNING) << "Crypto module " << crypto->GetID()
                     << " failed to decrypt.";
      }
      return plaintext;
    }
  }
  LOG(WARNING) << "Unable to decrypt text, returning as is.";
  return ciphertext;
}

}  // namespace shill
