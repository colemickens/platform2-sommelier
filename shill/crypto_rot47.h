//
// Copyright (C) 2011 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef SHILL_CRYPTO_ROT47_H_
#define SHILL_CRYPTO_ROT47_H_

#include <string>

#include <base/macros.h>

#include "shill/crypto_interface.h"

namespace shill {

// ROT47 crypto module implementation.
class CryptoRot47 : public CryptoInterface {
 public:
  static const char kID[];

  CryptoRot47();

  // Inherited from CryptoInterface.
  std::string GetID() override;
  bool Encrypt(const std::string& plaintext, std::string* ciphertext) override;
  bool Decrypt(const std::string& ciphertext, std::string* plaintext) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CryptoRot47);
};

}  // namespace shill

#endif  // SHILL_CRYPTO_ROT47_H_
