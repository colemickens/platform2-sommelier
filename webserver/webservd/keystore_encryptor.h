// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBSERVER_WEBSERVD_KEYSTORE_ENCRYPTOR_H_
#define WEBSERVER_WEBSERVD_KEYSTORE_ENCRYPTOR_H_

#include "webservd/encryptor.h"

#include <memory>
#include <string>

#include <keystore/keystore_client.h>

namespace webservd {

// An Encryptor implementation backed by Brillo Keystore. This class is intended
// to be the default encryptor on platforms that support it. An implementation
// of Encryptor::CreateDefaultEncryptor is provided for this class.
class KeystoreEncryptor : public Encryptor {
 public:
  explicit KeystoreEncryptor(
      std::unique_ptr<keystore::KeystoreClient> keystore);
  ~KeystoreEncryptor() override = default;

  bool EncryptWithAuthentication(const std::string& plaintext,
                                 std::string* ciphertext) override;
  bool DecryptWithAuthentication(const std::string& ciphertext,
                                 std::string* plaintext) override;

 private:
  std::unique_ptr<keystore::KeystoreClient> keystore_;
};

}  // namespace webservd

#endif  // WEBSERVER_WEBSERVD_KEYSTORE_ENCRYPTOR_H_
