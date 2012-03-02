// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/crypto_des_cbc.h"

#include <rpc/des_crypt.h>

#include <base/file_util.h>
#include <base/string_util.h>

#include "shill/glib.h"

using std::string;
using std::vector;

namespace shill {

const unsigned int CryptoDESCBC::kBlockSize = 8;
const char CryptoDESCBC::kID[] = "des-cbc";
const char CryptoDESCBC::kSentinel[] = "[ok]";
const char CryptoDESCBC::kVersion2Prefix[] = "02:";

CryptoDESCBC::CryptoDESCBC(GLib *glib) : glib_(glib) {}

string CryptoDESCBC::GetID() {
  return kID;
}

bool CryptoDESCBC::Encrypt(const string &plaintext, string *ciphertext) {
  CHECK_EQ(kBlockSize, key_.size());
  CHECK_EQ(kBlockSize, iv_.size());

  // Prepare the data to be encrypted by concatenating |plaintext| and
  // |kSentinel|, and appending nulls to a size multiple of |kBlockSize|.
  vector<char> data(plaintext.begin(), plaintext.end());
  data.insert(data.end(), kSentinel, kSentinel + strlen(kSentinel));
  // +1 to encrypt at least one null terminator. +(kBlockSize - 1) to pad to a
  // full block, if necessary.
  int blocks = (data.size() + 1 + (kBlockSize - 1)) / kBlockSize;
  data.resize(blocks * kBlockSize, '\0');

  // The IV is modified in place.
  vector<char> iv = iv_;
  int rv =
      cbc_crypt(key_.data(), data.data(), data.size(), DES_ENCRYPT, iv.data());
  if (DES_FAILED(rv)) {
    LOG(ERROR) << "DES-CBC encryption failed.";
    return false;
  }
  gchar *b64_ciphertext =
      glib_->Base64Encode(reinterpret_cast<guchar *>(data.data()), data.size());
  if (!b64_ciphertext) {
    LOG(ERROR) << "Unable to base64-encode DES-CBC ciphertext.";
    return false;
  }
  *ciphertext = string(kVersion2Prefix) + b64_ciphertext;
  glib_->Free(b64_ciphertext);
  return true;
}

bool CryptoDESCBC::Decrypt(const string &ciphertext, string *plaintext) {
  CHECK_EQ(kBlockSize, key_.size());
  CHECK_EQ(kBlockSize, iv_.size());
  int version = 1;
  string b64_ciphertext = ciphertext;
  if (StartsWithASCII(ciphertext, kVersion2Prefix, true)) {
    version = 2;
    b64_ciphertext.erase(0, strlen(kVersion2Prefix));
  }

  gsize data_len = 0;
  guchar *gdata = glib_->Base64Decode(b64_ciphertext.c_str(), &data_len);
  if (!gdata) {
    LOG(ERROR) << "Unable to base64-decode DEC-CBC ciphertext.";
    return false;
  }

  vector<char> data(gdata, gdata + data_len);
  glib_->Free(gdata);
  if (data.empty() || (data.size() % kBlockSize != 0)) {
    LOG(ERROR) << "Invalid DES-CBC ciphertext size: " << data.size();
    return false;
  }

  // The IV is modified in place.
  vector<char> iv = iv_;
  int rv =
      cbc_crypt(key_.data(), data.data(), data.size(), DES_DECRYPT, iv.data());
  if (DES_FAILED(rv)) {
    LOG(ERROR) << "DES-CBC decryption failed.";
    return false;
  }
  if (data.back() != '\0') {
    LOG(ERROR) << "DEC-CBC decryption resulted in invalid plain text.";
    return false;
  }
  string text = data.data();
  if (version == 2) {
    if (!EndsWith(text, kSentinel, true)) {
      LOG(ERROR) << "DES-CBC decrypted text missing sentinel -- bad key?";
      return false;
    }
    text.erase(text.size() - strlen(kSentinel), strlen(kSentinel));
  }
  *plaintext = text;
  return true;
}

bool CryptoDESCBC::LoadKeyMatter(const FilePath &path) {
  key_.clear();
  iv_.clear();
  string matter;
  // TODO(petkov): This mimics current flimflam behavior. Fix it so that it
  // doesn't read the whole file.
  if (!file_util::ReadFileToString(path, &matter)) {
    LOG(ERROR) << "Unable to load key matter from " << path.value();
    return false;
  }
  if (matter.size() < 2 * kBlockSize) {
    LOG(ERROR) << "Key matter data not enough " << matter.size() << " < "
               << 2 * kBlockSize;
    return false;
  }
  string::const_iterator matter_start =
      matter.begin() + (matter.size() - 2 * kBlockSize);
  key_.assign(matter_start + kBlockSize, matter_start + 2 * kBlockSize);
  iv_.assign(matter_start, matter_start + kBlockSize);
  return true;
}

}  // namespace shill
