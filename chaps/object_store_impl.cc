// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chaps/object_store_impl.h"

#include <map>
#include <string>

#include <base/logging.h>
#include <chromeos/utility.h>
#include <openssl/evp.h>

#include "chaps/chaps_utility.h"
#include "pkcs11/cryptoki.h"

using std::map;
using std::string;

namespace chaps {

const int ObjectStoreImpl::kAESBlockSizeBytes = 16;
const int ObjectStoreImpl::kAESKeySizeBytes = 32;
const int ObjectStoreImpl::kHMACSizeBytes = 64;

ObjectStoreImpl::ObjectStoreImpl() {
  EVP_CIPHER_CTX_init(&cipher_context_);
}

ObjectStoreImpl::~ObjectStoreImpl() {
  // TODO(dkrahn): Use SecureBlob. See crosbug.com/27681.
  chromeos::SecureMemset(const_cast<char*>(key_.data()), 0, key_.length());
  EVP_CIPHER_CTX_cleanup(&cipher_context_);
}

bool ObjectStoreImpl::Init(const FilePath& database_file) {
  return false;
}

bool ObjectStoreImpl::GetInternalBlob(int blob_id, string* blob) {
  return false;
}

bool ObjectStoreImpl::SetInternalBlob(int blob_id, const string& blob) {
  return false;
}

bool ObjectStoreImpl::SetEncryptionKey(const string& key) {
  if (key.length() != static_cast<size_t>(kAESKeySizeBytes)) {
    LOG(ERROR) << "Unexpected key size: " << key.length();
    return false;
  }
  key_ = key;
  return true;
}

bool ObjectStoreImpl::InsertObjectBlob(bool is_private,
                                       CK_OBJECT_CLASS object_class,
                                       const string& key_id,
                                       const string& blob,
                                       int* handle) {
  return false;
}

bool ObjectStoreImpl::DeleteObjectBlob(int handle) {
  return false;
}

bool ObjectStoreImpl::UpdateObjectBlob(int handle, const string& blob) {
  return false;
}

bool ObjectStoreImpl::LoadAllObjectBlobs(map<int, string>* blobs) {
  return false;
}

bool ObjectStoreImpl::RunCipher(bool is_encrypt,
                                const string& input,
                                string* output) {
  if (key_.empty()) {
    LOG(ERROR) << "The store encryption key has not been initialized.";
    return false;
  }
  int input_length = input.length();
  string iv;
  if (is_encrypt) {
    // Generate a new random IV.
    iv.resize(kAESBlockSizeBytes);
    RAND_bytes(ConvertStringToByteBuffer(iv.data()), kAESBlockSizeBytes);
  } else {
    // Recover the IV from the input.
    if (input_length < kAESBlockSizeBytes) {
      LOG(ERROR) << "Decrypt: Invalid input.";
      return false;
    }
    iv = input.substr(input_length - kAESBlockSizeBytes);
    input_length -= kAESBlockSizeBytes;
  }
  if (!EVP_CipherInit_ex(&cipher_context_,
                         EVP_aes_256_cbc(),
                         NULL,
                         ConvertStringToByteBuffer(key_.data()),
                         ConvertStringToByteBuffer(iv.data()),
                         is_encrypt)) {
    LOG(ERROR) << "EVP_CipherInit_ex failed: " << GetOpenSSLError();
    return false;
  }
  EVP_CIPHER_CTX_set_padding(&cipher_context_,
                             1);  // Enables PKCS padding.
  // Set the buffer size to be large enough to hold all output. For encryption,
  // this will allow space for padding and, for decryption, this will comply
  // with openssl documentation (even though the final output will be no larger
  // than input_length).
  output->resize(input_length + kAESBlockSizeBytes);
  int output_length = 0;
  unsigned char* output_bytes = ConvertStringToByteBuffer(output->data());
  unsigned char* input_bytes = ConvertStringToByteBuffer(input.data());
  if (!EVP_CipherUpdate(&cipher_context_,
                        output_bytes,
                        &output_length,  // Will be set to actual output length.
                        input_bytes,
                        input_length)) {
    LOG(ERROR) << "EVP_CipherUpdate failed: " << GetOpenSSLError();
    return false;
  }
  // The final block is yet to be computed. This check ensures we have at least
  // kAESBlockSizeBytes bytes left in the output buffer.
  CHECK(output_length <= input_length);
  int output_length2 = 0;
  if (!EVP_CipherFinal_ex(&cipher_context_,
                          output_bytes + output_length,
                          &output_length2)) {
    LOG(ERROR) << "EVP_CipherFinal_ex failed: " << GetOpenSSLError();
    return false;
  }
  // Adjust the output size to the number of bytes actually written.
  output->resize(output_length + output_length2);
  if (is_encrypt) {
    // Append the IV so it will be available during decryption.
    *output += iv;
  }
  return true;
}

bool ObjectStoreImpl::Encrypt(const string& plain_text,
                              string* cipher_text) {
  return RunCipher(true, AppendHMAC(plain_text), cipher_text);
}

bool ObjectStoreImpl::Decrypt(const string& cipher_text,
                              string* plain_text) {
  string plain_text_with_hmac;
  if (!RunCipher(false, cipher_text, &plain_text_with_hmac))
    return false;
  if (!VerifyAndStripHMAC(plain_text_with_hmac, plain_text))
    return false;
  return true;
}

string ObjectStoreImpl::AppendHMAC(const string& input) {
  return input + HmacSha512(input, key_);
}

bool ObjectStoreImpl::VerifyAndStripHMAC(const string& input,
                                         string* stripped) {
  if (input.size() < static_cast<size_t>(kHMACSizeBytes)) {
    LOG(ERROR) << "Failed to verify blob integrity.";
    return false;
  }
  *stripped = input.substr(0, input.size() - kHMACSizeBytes);
  string hmac = input.substr(input.size() - kHMACSizeBytes);
  if (hmac != HmacSha512(*stripped, key_)) {
    LOG(ERROR) << "Failed to verify blob integrity.";
    return false;
  }
  return true;
}

}  // namespace chaps
