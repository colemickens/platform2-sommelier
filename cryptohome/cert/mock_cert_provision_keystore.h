// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Mock for KeyStore.

#ifndef CRYPTOHOME_CERT_MOCK_CERT_PROVISION_KEYSTORE_H_
#define CRYPTOHOME_CERT_MOCK_CERT_PROVISION_KEYSTORE_H_

#include <string>

#include "cryptohome/cert/cert_provision_keystore.h"

namespace cert_provision {

class MockKeyStore : public KeyStore {
 public:
  MockKeyStore() = default;
  MOCK_METHOD(OpResult, Init, (), (override));
  MOCK_METHOD(void, TearDown, (), (override));
  MOCK_METHOD(OpResult,
              Sign,
              (const std::string&,
               const std::string&,
               SignMechanism,
               const std::string&,
               std::string*),
              (override));
  MOCK_METHOD(OpResult,
              ReadProvisionStatus,
              (const std::string&, google::protobuf::MessageLite*),
              (override));
  MOCK_METHOD(OpResult,
              WriteProvisionStatus,
              (const std::string&, const google::protobuf::MessageLite&),
              (override));
  MOCK_METHOD(OpResult,
              DeleteKeys,
              (const std::string&, const std::string&),
              (override));
};

}  // namespace cert_provision

#endif  // CRYPTOHOME_CERT_MOCK_CERT_PROVISION_KEYSTORE_H_
