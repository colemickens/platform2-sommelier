// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CRYPTO_DES_CBC_
#define SHILL_CRYPTO_DES_CBC_

#include <vector>

#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/crypto_interface.h"

class FilePath;

namespace shill {

class GLib;

// DES-CBC crypto module implementation.
class CryptoDESCBC : public CryptoInterface {
 public:
  static const char kID[];

  CryptoDESCBC(GLib *glib);

  // Sets the DES key to the last |kBlockSize| bytes of |key_matter_path| and
  // the DES initialization vector to the second to last |kBlockSize| bytes of
  // |key_matter_path|. Returns true on success.
  bool LoadKeyMatter(const FilePath &path);

  // Inherited from CryptoInterface.
  virtual std::string GetID();
  virtual bool Encrypt(const std::string &plaintext, std::string *ciphertext);
  virtual bool Decrypt(const std::string &ciphertext, std::string *plaintext);

  const std::vector<char> &key() const { return key_; }
  const std::vector<char> &iv() const { return iv_; }

 private:
  FRIEND_TEST(CryptoDESCBCTest, Decrypt);
  FRIEND_TEST(CryptoDESCBCTest, Encrypt);

  static const int kBlockSize;
  static const char kSentinel[];
  static const char kVersion2Prefix[];

  GLib *glib_;
  std::vector<char> key_;
  std::vector<char> iv_;
};

}  // namespace shill

#endif  // SHILL_CRYPTO_DES_CBC_
