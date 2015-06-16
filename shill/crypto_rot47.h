// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CRYPTO_ROT47_H_
#define SHILL_CRYPTO_ROT47_H_

#include <string>

#include <base/macros.h>

#include "shill/crypto_interface.h"

namespace shill {

// ROT47 crypto module implementation.
class CryptoROT47 : public CryptoInterface {
 public:
  static const char kID[];

  CryptoROT47();

  // Inherited from CryptoInterface.
  virtual std::string GetID();
  virtual bool Encrypt(const std::string& plaintext, std::string* ciphertext);
  virtual bool Decrypt(const std::string& ciphertext, std::string* plaintext);

 private:
  DISALLOW_COPY_AND_ASSIGN(CryptoROT47);
};

}  // namespace shill

#endif  // SHILL_CRYPTO_ROT47_H_
