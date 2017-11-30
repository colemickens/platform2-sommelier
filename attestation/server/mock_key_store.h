// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
  virtual ~MockKeyStore();

  MOCK_METHOD3(Read, bool(const std::string& username,
                          const std::string& name,
                          std::string* key_data));
  MOCK_METHOD3(Write, bool(const std::string& username,
                           const std::string& name,
                           const std::string& key_data));
  MOCK_METHOD2(Delete, bool(const std::string& username,
                            const std::string& name));
  MOCK_METHOD2(DeleteByPrefix, bool(const std::string& username,
                                    const std::string& key_prefix));
  MOCK_METHOD7(Register, bool(const std::string& username,
                              const std::string& label,
                              KeyType key_type,
                              KeyUsage key_usage,
                              const std::string& private_key_blob,
                              const std::string& public_key_der,
                              const std::string& certificate));
  MOCK_METHOD2(RegisterCertificate, bool(const std::string& username,
                                         const std::string& certificate));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockKeyStore);
};

}  // namespace attestation

#endif  // ATTESTATION_SERVER_MOCK_KEY_STORE_H_
