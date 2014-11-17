// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "privetd/security_delegate.h"

#include <algorithm>

#include <openssl/evp.h>
#include <openssl/hmac.h>

#include <base/logging.h>
#include <base/rand_util.h>
#include <base/stl_util.h>
#include <base/strings/string_number_conversions.h>
#include <chromeos/secure_blob.h>
#include <chromeos/strings/string_utils.h>

namespace privetd {

namespace {

const char kTokenDelimeter = ':';
const size_t kSha256OutputSize = 32;

chromeos::SecureBlob HmacSha256(const chromeos::SecureBlob& key,
                                const chromeos::Blob& data) {
  chromeos::SecureBlob mac(kSha256OutputSize);
  uint32_t len = 0;
  CHECK(HMAC(EVP_sha256(), vector_as_array(&key), key.size(),
             vector_as_array(&data), data.size(), vector_as_array(&mac), &len));
  CHECK_EQ(len, kSha256OutputSize);
  return mac;
}

std::string Base64Encode(const chromeos::Blob& input) {
  BIO* base64 = BIO_new(BIO_f_base64());
  BIO* bio = BIO_new(BIO_s_mem());
  bio = BIO_push(base64, bio);
  BIO_write(bio, input.data(), input.size());
  static_cast<void>(BIO_flush(bio));
  char* data = nullptr;
  size_t length = BIO_get_mem_data(bio, &data);
  std::string output(data, length);
  chromeos::SecureMemset(data, 0, length);
  BIO_free_all(bio);
  return output;
}

chromeos::Blob Base64Decode(std::string input) {
  chromeos::Blob result(input.size(), 0);
  BIO* base64 = BIO_new(BIO_f_base64());
  BIO* bio = BIO_new_mem_buf(&input.front(), input.length());
  bio = BIO_push(base64, bio);
  int size = BIO_read(bio, result.data(), input.size());
  BIO_free_all(bio);
  result.resize(std::max<int>(0, size));
  return result;
}

// Returns "scope:time".
std::string CreateTokenData(AuthScope scope, const base::Time& time) {
  return base::IntToString(static_cast<int>(scope)) + kTokenDelimeter +
         base::Int64ToString(time.ToTimeT());
}

// Splits string of "scope:time" format.
AuthScope SplitTokenData(const std::string& token, base::Time* time) {
  auto parts = chromeos::string_utils::SplitAtFirst(token, kTokenDelimeter);
  int scope = 0;
  if (!base::StringToInt(parts.first, &scope) ||
      scope < static_cast<int>(AuthScope::kNone) ||
      scope > static_cast<int>(AuthScope::kOwner)) {
    return AuthScope::kNone;
  }

  int64_t timestamp = 0;
  if (!base::StringToInt64(parts.second, &timestamp))
    return AuthScope::kNone;
  *time = base::Time::FromTimeT(timestamp);
  return static_cast<AuthScope>(scope);
}

class SecurityDelegateImpl : public SecurityDelegate {
 public:
  SecurityDelegateImpl() : secret_(kSha256OutputSize) {
    base::RandBytes(secret_.data(), kSha256OutputSize);
  }
  ~SecurityDelegateImpl() override {}

  // SecurityDelegate methods
  // Returns "base64([hmac]scope:time)".
  std::string CreateAccessToken(AuthScope scope,
                                const base::Time& time) const override {
    chromeos::SecureBlob data(CreateTokenData(scope, time));
    chromeos::SecureBlob hash(HmacSha256(secret_, data));
    return Base64Encode(chromeos::SecureBlob::Combine(hash, data));
  }

  // Parses "base64([hmac]scope:time)".
  AuthScope ParseAccessToken(const std::string& token,
                             base::Time* time) const override {
    chromeos::Blob decoded = Base64Decode(token);
    if (decoded.size() <= kSha256OutputSize)
      return AuthScope::kNone;
    chromeos::SecureBlob data(decoded.begin() + kSha256OutputSize,
                              decoded.end());
    decoded.resize(kSha256OutputSize);
    if (decoded != HmacSha256(secret_, data))
      return AuthScope::kNone;
    return SplitTokenData(data.to_string(), time);
  }

  std::vector<PairingType> GetPairingTypes() const override {
    return {PairingType::kEmbeddedCode};
  }

  bool IsValidPairingCode(const std::string& auth_code) const override {
    return auth_code == (device_commitment_ + client_commitment_);
  }

  Error StartPairing(PairingType mode,
                     std::string* session_id,
                     std::string* device_commitment) override {
    *session_id = session_id_;
    *device_commitment = device_commitment_;
    return Error::kNone;
  }

  Error ConfirmPairing(const std::string& sessionId,
                       const std::string& client_commitment,
                       std::string* fingerprint,
                       std::string* signature) override {
    client_commitment_ = client_commitment;
    *fingerprint = "fingerprint";
    *signature = "signature";
    return Error::kNone;
  }

 private:
  std::string session_id_ = "111";
  std::string client_commitment_;
  std::string device_commitment_ = "1234";
  chromeos::SecureBlob secret_;
};

}  // namespace

SecurityDelegate::SecurityDelegate() {
}

SecurityDelegate::~SecurityDelegate() {
}

// static
std::unique_ptr<SecurityDelegate> SecurityDelegate::CreateDefault() {
  return std::unique_ptr<SecurityDelegate>(new SecurityDelegateImpl);
}

}  // namespace privetd
