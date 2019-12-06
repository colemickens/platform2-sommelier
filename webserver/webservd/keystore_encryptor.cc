// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webservd/keystore_encryptor.h"

#include <memory>
#include <utility>

#include <keystore/keystore_client_impl.h>

namespace {

const char kWebserverKeyName[] = "webservd_https_a40cd1b4";

}  // namespace

namespace webservd {

std::unique_ptr<Encryptor> Encryptor::CreateDefaultEncryptor() {
  return std::unique_ptr<Encryptor>(
      new KeystoreEncryptor(std::unique_ptr<keystore::KeystoreClient>(
          new keystore::KeystoreClientImpl)));
}

KeystoreEncryptor::KeystoreEncryptor(
    std::unique_ptr<keystore::KeystoreClient> keystore)
    : keystore_(std::move(keystore)) {}

bool KeystoreEncryptor::EncryptWithAuthentication(const std::string& plaintext,
                                                  std::string* ciphertext) {
  return keystore_->encryptWithAuthentication(kWebserverKeyName, plaintext,
                                              ciphertext);
}

bool KeystoreEncryptor::DecryptWithAuthentication(const std::string& ciphertext,
                                                  std::string* plaintext) {
  return keystore_->decryptWithAuthentication(kWebserverKeyName, ciphertext,
                                              plaintext);
}

}  // namespace webservd
