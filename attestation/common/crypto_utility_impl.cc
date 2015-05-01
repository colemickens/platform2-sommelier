// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "attestation/common/crypto_utility_impl.h"

#include <limits>
#include <string>

#include <base/stl_util.h>
#include <crypto/scoped_openssl_types.h>
#include <crypto/secure_util.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>
#include <openssl/x509.h>

namespace {

const size_t kAesKeySize = 32;
const size_t kAesBlockSize = 16;

std::string GetOpenSSLError() {
  BIO* bio = BIO_new(BIO_s_mem());
  ERR_print_errors(bio);
  char* data = nullptr;
  int data_len = BIO_get_mem_data(bio, &data);
  std::string error_string(data, data_len);
  BIO_free(bio);
  return error_string;
}

unsigned char* StringAsOpenSSLBuffer(std::string* s) {
  return reinterpret_cast<unsigned char*>(string_as_array(s));
}

}  // namespace

namespace attestation {

CryptoUtilityImpl::CryptoUtilityImpl(TpmUtility* tpm_utility)
    : tpm_utility_(tpm_utility) {
  OpenSSL_add_all_algorithms();
  ERR_load_crypto_strings();
}

CryptoUtilityImpl::~CryptoUtilityImpl() {
  EVP_cleanup();
  ERR_free_strings();
}

bool CryptoUtilityImpl::GetRandom(size_t num_bytes,
                                  std::string* random_data) const {
  // OpenSSL takes a signed integer.
  if (num_bytes > static_cast<size_t>(std::numeric_limits<int>::max())) {
    return false;
  }
  random_data->resize(num_bytes);
  unsigned char* buffer = StringAsOpenSSLBuffer(random_data);
  return (RAND_bytes(buffer, num_bytes) == 1);
}

bool CryptoUtilityImpl::CreateSealedKey(std::string* aes_key,
                                        std::string* sealed_key) {
  if (!GetRandom(kAesKeySize, aes_key)) {
    LOG(ERROR) << __func__ << ": GetRandom failed.";
    return false;
  }
  if (!tpm_utility_->SealToPCR0(*aes_key, sealed_key)) {
    LOG(ERROR) << __func__ << ": Failed to seal cipher key.";
    return false;
  }
  return true;
}

bool CryptoUtilityImpl::EncryptData(const std::string& data,
                                    const std::string& aes_key,
                                    const std::string& sealed_key,
                                    std::string* encrypted_data) {
  std::string iv;
  if (!GetRandom(kAesBlockSize, &iv)) {
    LOG(ERROR) << __func__ << ": GetRandom failed.";
    return false;
  }
  std::string raw_encrypted_data;
  if (!AesEncrypt(data, aes_key, iv, &raw_encrypted_data)) {
    LOG(ERROR) << __func__ << ": AES encryption failed.";
    return false;
  }
  EncryptedData encrypted_pb;
  encrypted_pb.set_wrapped_key(sealed_key);
  encrypted_pb.set_iv(iv);
  encrypted_pb.set_encrypted_data(raw_encrypted_data);
  encrypted_pb.set_mac(HmacSha512(iv + raw_encrypted_data, aes_key));
  if (!encrypted_pb.SerializeToString(encrypted_data)) {
    LOG(ERROR) << __func__ << ": Failed to serialize protobuf.";
    return false;
  }
  return true;
}

bool CryptoUtilityImpl::UnsealKey(const std::string& encrypted_data,
                                  std::string* aes_key,
                                  std::string* sealed_key) {
  EncryptedData encrypted_pb;
  if (!encrypted_pb.ParseFromString(encrypted_data)) {
    LOG(ERROR) << __func__ << ": Failed to parse protobuf.";
    return false;
  }
  *sealed_key = encrypted_pb.wrapped_key();
  if (!tpm_utility_->Unseal(*sealed_key, aes_key)) {
    LOG(ERROR) << __func__ << ": Cannot unseal aes key.";
    return false;
  }
  return true;
}

bool CryptoUtilityImpl::DecryptData(const std::string& encrypted_data,
                                    const std::string& aes_key,
                                    std::string* data) {
  EncryptedData encrypted_pb;
  if (!encrypted_pb.ParseFromString(encrypted_data)) {
    LOG(ERROR) << __func__ << ": Failed to parse protobuf.";
    return false;
  }
  std::string mac = HmacSha512(
      encrypted_pb.iv() + encrypted_pb.encrypted_data(),
      aes_key);
  if (mac.length() != encrypted_pb.mac().length()) {
    LOG(ERROR) << __func__ << ": Corrupted data in encrypted pb.";
    return false;
  }
  if (!crypto::SecureMemEqual(mac.data(), encrypted_pb.mac().data(),
                              mac.length())) {
    LOG(ERROR) << __func__ << ": Corrupted data in encrypted pb.";
    return false;
  }
  if (!AesDecrypt(encrypted_pb.encrypted_data(), aes_key, encrypted_pb.iv(),
                  data)) {
    LOG(ERROR) << __func__ << ": AES decryption failed.";
    return false;
  }
  return true;
}

bool CryptoUtilityImpl::GetRSASubjectPublicKeyInfo(
    const std::string& public_key,
    std::string* spki) {
  const unsigned char* asn1_ptr = reinterpret_cast<const unsigned char*>(
      public_key.data());
  crypto::ScopedRSA rsa(d2i_RSAPublicKey(nullptr, &asn1_ptr,
                                         public_key.size()));
  if (!rsa.get()) {
    LOG(ERROR) << __func__ << ": Failed to decode public key.";
    return false;
  }
  unsigned char* buffer = nullptr;
  int length = i2d_RSA_PUBKEY(rsa.get(), &buffer);
  if (length <= 0) {
    LOG(ERROR) << __func__ << ": Failed to encode public key.";
    return false;
  }
  crypto::ScopedOpenSSLBytes scoped_buffer(buffer);
  spki->assign(reinterpret_cast<char*>(buffer), length);
  return true;
}

bool CryptoUtilityImpl::AesEncrypt(const std::string& data,
                                   const std::string& key,
                                   const std::string& iv,
                                   std::string* encrypted_data) {
  if (key.size() != kAesKeySize || iv.size() != kAesBlockSize) {
    return false;
  }
  if (data.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
    // EVP_EncryptUpdate takes a signed int.
    return false;
  }
  std::string mutable_data(data);
  unsigned char* input_buffer = StringAsOpenSSLBuffer(&mutable_data);
  std::string mutable_key(key);
  unsigned char* key_buffer = StringAsOpenSSLBuffer(&mutable_key);
  std::string mutable_iv(iv);
  unsigned char* iv_buffer = StringAsOpenSSLBuffer(&mutable_iv);
  // Allocate enough space for the output (including padding).
  encrypted_data->resize(data.size() + kAesBlockSize);
  unsigned char* output_buffer = reinterpret_cast<unsigned char*>(
      string_as_array(encrypted_data));
  int output_size = 0;
  const EVP_CIPHER* cipher = EVP_aes_256_cbc();
  EVP_CIPHER_CTX encryption_context;
  EVP_CIPHER_CTX_init(&encryption_context);
  if (!EVP_EncryptInit_ex(&encryption_context, cipher, nullptr, key_buffer,
                          iv_buffer)) {
    LOG(ERROR) << __func__ << ": " << GetOpenSSLError();
    return false;
  }
  if (!EVP_EncryptUpdate(&encryption_context, output_buffer, &output_size,
                         input_buffer, data.size())) {
    LOG(ERROR) << __func__ << ": " << GetOpenSSLError();
    EVP_CIPHER_CTX_cleanup(&encryption_context);
    return false;
  }
  size_t total_size = output_size;
  output_buffer += output_size;
  output_size = 0;
  if (!EVP_EncryptFinal_ex(&encryption_context, output_buffer, &output_size)) {
    LOG(ERROR) << __func__ << ": " << GetOpenSSLError();
    EVP_CIPHER_CTX_cleanup(&encryption_context);
    return false;
  }
  total_size += output_size;
  encrypted_data->resize(total_size);
  EVP_CIPHER_CTX_cleanup(&encryption_context);
  return true;
}

bool CryptoUtilityImpl::AesDecrypt(const std::string& encrypted_data,
                                   const std::string& key,
                                   const std::string& iv,
                                   std::string* data) {
  if (key.size() != kAesKeySize || iv.size() != kAesBlockSize) {
    return false;
  }
  if (encrypted_data.size() >
      static_cast<size_t>(std::numeric_limits<int>::max())) {
    // EVP_DecryptUpdate takes a signed int.
    return false;
  }
  std::string mutable_encrypted_data(encrypted_data);
  unsigned char* input_buffer = StringAsOpenSSLBuffer(&mutable_encrypted_data);
  std::string mutable_key(key);
  unsigned char* key_buffer = StringAsOpenSSLBuffer(&mutable_key);
  std::string mutable_iv(iv);
  unsigned char* iv_buffer = StringAsOpenSSLBuffer(&mutable_iv);
  // Allocate enough space for the output.
  data->resize(encrypted_data.size());
  unsigned char* output_buffer = StringAsOpenSSLBuffer(data);
  int output_size = 0;
  const EVP_CIPHER* cipher = EVP_aes_256_cbc();
  EVP_CIPHER_CTX decryption_context;
  EVP_CIPHER_CTX_init(&decryption_context);
  if (!EVP_DecryptInit_ex(&decryption_context, cipher, nullptr, key_buffer,
                          iv_buffer)) {
    LOG(ERROR) << __func__ << ": " << GetOpenSSLError();
    return false;
  }
  if (!EVP_DecryptUpdate(&decryption_context, output_buffer, &output_size,
                         input_buffer, encrypted_data.size())) {
    LOG(ERROR) << __func__ << ": " << GetOpenSSLError();
    EVP_CIPHER_CTX_cleanup(&decryption_context);
    return false;
  }
  size_t total_size = output_size;
  output_buffer += output_size;
  output_size = 0;
  if (!EVP_DecryptFinal_ex(&decryption_context, output_buffer, &output_size)) {
    LOG(ERROR) << __func__ << ": " << GetOpenSSLError();
    EVP_CIPHER_CTX_cleanup(&decryption_context);
    return false;
  }
  total_size += output_size;
  data->resize(total_size);
  EVP_CIPHER_CTX_cleanup(&decryption_context);
  return true;
}

std::string CryptoUtilityImpl::HmacSha512(const std::string& data,
                                          const std::string& key) {
  const int kSha512OutputSize = 64;
  unsigned char mac[kSha512OutputSize];
  std::string mutable_data(data);
  unsigned char* data_buffer = StringAsOpenSSLBuffer(&mutable_data);
  HMAC(EVP_sha512(), key.data(), key.size(), data_buffer, data.size(), mac,
       nullptr);
  return std::string(std::begin(mac), std::end(mac));
}

}  // namespace attestation
