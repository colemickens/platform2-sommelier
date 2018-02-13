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
  MOCK_METHOD0(Init, OpResult());
  MOCK_METHOD0(TearDown, void());
  MOCK_METHOD5(Sign,
               OpResult(const std::string&,
                        const std::string&,
                        SignMechanism,
                        const std::string&,
                        std::string*));
  MOCK_METHOD2(ReadProvisionStatus,
               OpResult(const std::string&, google::protobuf::MessageLite*));
  MOCK_METHOD2(WriteProvisionStatus,
               OpResult(const std::string&,
                        const google::protobuf::MessageLite&));
  MOCK_METHOD2(DeleteKeys, OpResult(const std::string&, const std::string&));
};

}  // namespace cert_provision

#endif  // CRYPTOHOME_CERT_MOCK_CERT_PROVISION_KEYSTORE_H_
