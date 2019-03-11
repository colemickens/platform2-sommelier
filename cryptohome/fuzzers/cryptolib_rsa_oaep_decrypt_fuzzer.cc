// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <cstring>
#include <vector>

#include <base/logging.h>
#include <brillo/secure_blob.h>
#include <crypto/scoped_openssl_types.h>
#include <openssl/err.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>

#include "cryptohome/cryptolib.h"

using brillo::Blob;
using brillo::SecureBlob;
using crypto::ScopedRSA;
using cryptohome::CryptoLib;

namespace {

struct ScopedOpensslErrorClearer {
  ~ScopedOpensslErrorClearer() { ERR_clear_error(); }
};

bool SplitFuzzerInput(const uint8_t* data,
                      size_t data_size,
                      size_t piece_count,
                      std::vector<Blob>* pieces) {
  pieces->resize(piece_count);
  size_t byte_index = 0;
  for (size_t piece_number = 0; piece_number < piece_count; ++piece_number) {
    if (byte_index + 4 > data_size)
      return false;
    const uint32_t piece_size =
        (static_cast<uint32_t>(data[byte_index]) << 24) +
        (static_cast<uint32_t>(data[byte_index + 1]) << 16) +
        (static_cast<uint32_t>(data[byte_index + 2]) << 8) +
        data[byte_index + 3];
    byte_index += 4;
    if (piece_size > data_size - byte_index)
      return false;
    (*pieces)[piece_number].assign(data + byte_index,
                                   data + byte_index + piece_size);
    byte_index += piece_size;
  }
  return true;
}

bool ParseFuzzerParameters(const uint8_t* data,
                           size_t data_size,
                           SecureBlob* ciphertext,
                           SecureBlob* oaep_label,
                           ScopedRSA* rsa) {
  std::vector<Blob> input_pieces;
  if (!SplitFuzzerInput(data, data_size, 3, &input_pieces))
    return false;
  ciphertext->assign(input_pieces[0].begin(), input_pieces[0].end());
  oaep_label->assign(input_pieces[1].begin(), input_pieces[1].end());
  const unsigned char* rsa_parsing_begin = input_pieces[2].data();
  rsa->reset(
      d2i_RSAPrivateKey(nullptr, &rsa_parsing_begin, input_pieces[2].size()));
  if (!*rsa)
    return false;
  return true;
}

struct Environment {
  Environment() {
    logging::SetMinLogLevel(logging::LOG_FATAL);
  }
};

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Environment environment;
  // Prevent OpenSSL errors from accumulating in the error queue and leaking the
  // memory across fuzzer executions.
  ScopedOpensslErrorClearer scoped_openssl_error_clearer;

  SecureBlob ciphertext;
  SecureBlob oaep_label;
  ScopedRSA rsa;
  if (!ParseFuzzerParameters(data, size, &ciphertext, &oaep_label, &rsa))
    return 0;

  SecureBlob decrypted_data;
  CryptoLib::RsaOaepDecrypt(ciphertext, oaep_label, rsa.get(), &decrypted_data);
  return 0;
}
