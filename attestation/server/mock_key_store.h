//
// Copyright (C) 2015 The Android Open Source Project
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

#ifndef ATTESTATION_SERVER_MOCK_KEY_STORE_H_
#define ATTESTATION_SERVER_MOCK_KEY_STORE_H_

#include "attestation/server/key_store.h"

#include <string>

#include <base/macros.h>
#include <gmock/gmock.h>

namespace attestation {

class MockKeyStore : public KeyStore {
 public:
  MockKeyStore();
  ~MockKeyStore() override;

  MOCK_METHOD(bool,
              Read,
              (const std::string&, const std::string&, std::string*),
              (override));
  MOCK_METHOD(bool,
              Write,
              (const std::string&, const std::string&, const std::string&),
              (override));
  MOCK_METHOD(bool,
              Delete,
              (const std::string&, const std::string&),
              (override));
  MOCK_METHOD(bool,
              DeleteByPrefix,
              (const std::string&, const std::string&),
              (override));
  MOCK_METHOD(bool,
              Register,
              (const std::string&,
               const std::string&,
               KeyType,
               KeyUsage,
               const std::string&,
               const std::string&,
               const std::string&),
              (override));
  MOCK_METHOD(bool,
              RegisterCertificate,
              (const std::string&, const std::string&),
              (override));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockKeyStore);
};

}  // namespace attestation

#endif  // ATTESTATION_SERVER_MOCK_KEY_STORE_H_
