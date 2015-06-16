// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/crypto_rot47.h"

using std::string;

namespace shill {

const char CryptoROT47::kID[] = "rot47";

CryptoROT47::CryptoROT47() {}

string CryptoROT47::GetID() {
  return kID;
}

bool CryptoROT47::Encrypt(const string& plaintext, string* ciphertext) {
  const int kRotSize = 94;
  const int kRotHalf = kRotSize / 2;
  const char kRotMin = '!';
  const char kRotMax = kRotMin + kRotSize - 1;

  *ciphertext = plaintext;
  for (auto& ch : *ciphertext) {
    if (ch < kRotMin || ch > kRotMax) {
      continue;
    }
    int rot = ch + kRotHalf;
    ch = (rot > kRotMax) ? rot - kRotSize : rot;
  }
  return true;
}

bool CryptoROT47::Decrypt(const string& ciphertext, string* plaintext) {
  // ROT47 is self-reciprocal.
  return Encrypt(ciphertext, plaintext);
}

}  // namespace shill
