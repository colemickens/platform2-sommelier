// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CRYPTO_ROT47_
#define SHILL_CRYPTO_ROT47_

#include "shill/crypto_interface.h"

namespace shill {

// ROT47 crypto module implementation.
class CryptoROT47 : public CryptoInterface {
 public:
  static const char kID[];

  // Inherited from CryptoInterface.
  virtual std::string GetID();
  virtual bool Encrypt(const std::string &plaintext, std::string *ciphertext);
  virtual bool Decrypt(const std::string &ciphertext, std::string *plaintext);
};

}  // namespace shill

#endif  // SHILL_CRYPTO_ROT47_
