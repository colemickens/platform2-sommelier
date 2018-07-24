// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CRYPTO_PROVIDER_H_
#define SHILL_CRYPTO_PROVIDER_H_

#include <string>

#include <base/files/file_path.h>
#include <base/memory/scoped_vector.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/crypto_interface.h"

namespace shill {

class CryptoProvider {
 public:
  CryptoProvider();

  void Init();

  // Returns |plaintext| encrypted by the highest priority available crypto
  // module capable of performing the operation. If no module succeeds, returns
  // |plaintext| as is.
  std::string Encrypt(const std::string& plaintext);

  // Returns |ciphertext| decrypted by the highest priority available crypto
  // module capable of performing the operation. If no module succeeds, returns
  // |ciphertext| as is.
  std::string Decrypt(const std::string& ciphertext);

  void set_key_matter_file(const base::FilePath& path) {
    key_matter_file_ = path;
  }

 private:
  FRIEND_TEST(CryptoProviderTest, Init);
  FRIEND_TEST(KeyFileStoreTest, OpenClose);
  typedef ScopedVector<CryptoInterface> Cryptos;

  static const char kKeyMatterFile[];

  // Registered crypto modules in high to low priority order.
  Cryptos cryptos_;

  base::FilePath key_matter_file_;

  DISALLOW_COPY_AND_ASSIGN(CryptoProvider);
};

}  // namespace shill

#endif  // SHILL_CRYPTO_PROVIDER_H_
