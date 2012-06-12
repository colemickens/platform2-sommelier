// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// SHA FIPS-180 Test Vector Class

#ifndef CRYPTOHOME_SHA_TEST_VECTORS_H_
#define CRYPTOHOME_SHA_TEST_VECTORS_H_

#include <openssl/sha.h>

#include <base/basictypes.h>
#include <chromeos/secure_blob.h>
#include <chromeos/utility.h>

namespace cryptohome {

// FIPS 180-2 test vectors for SHA-1 and SHA-256
class ShaTestVectors {
 public:
  explicit ShaTestVectors(int type);

  ~ShaTestVectors() { }
  const chromeos::Blob* input(int index) const { return &input_[index]; }
  const chromeos::SecureBlob* output(int index) const { return &output_[index];}
  size_t count() const { return 3; }  // sizeof(input_); }

  static const char* kOneBlockMessage;
  static const char* kMultiBlockMessage;
  static const uint8_t kSha1Results[][SHA_DIGEST_LENGTH];
  static const uint8_t kSha256Results[][SHA256_DIGEST_LENGTH];
 private:
  chromeos::Blob input_[3];
  chromeos::SecureBlob output_[3];
};

}  // namespace cryptohome
#endif  // CRYPTOHOME_SHA_TEST_VECTORS_H_
