// Copyright 2015 The Android Open Source Project
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

#include "webservd/fake_encryptor.h"

#include <chromeos/data_encoding.h>

namespace webservd {

bool FakeEncryptor::EncryptString(const std::string& plaintext,
                                  std::string* ciphertext) {
  *ciphertext = chromeos::data_encoding::Base64Encode(plaintext);
  return true;
}

bool FakeEncryptor::DecryptString(const std::string& ciphertext,
                                  std::string* plaintext) {
  return chromeos::data_encoding::Base64Decode(ciphertext, plaintext);
}

}  // namespace webservd
