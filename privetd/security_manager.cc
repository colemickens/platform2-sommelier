// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "privetd/security_manager.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <vector>

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>

#include <base/logging.h>
#include <base/rand_util.h>
#include <base/stl_util.h>
#include <base/strings/string_number_conversions.h>
#include <base/time/time.h>
#include <chromeos/strings/string_utils.h>

#include "privetd/constants.h"
#include "privetd/openssl_utils.h"

namespace privetd {

namespace {

const char kTokenDelimeter = ':';

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

std::unique_ptr<X509, void(*)(X509*)> CreateCertificate(
    int serial_number,
    const base::TimeDelta& cert_expiration,
    const std::string& common_name) {
  auto cert = std::unique_ptr<X509, void(*)(X509*)>{X509_new(), X509_free};
  CHECK(cert.get());
  X509_set_version(cert.get(), 2);  // Using X.509 v3 certificate...

  // Set certificate properties...
  ASN1_INTEGER* sn = X509_get_serialNumber(cert.get());
  ASN1_INTEGER_set(sn, serial_number);
  X509_gmtime_adj(X509_get_notBefore(cert.get()), 0);
  X509_gmtime_adj(X509_get_notAfter(cert.get()), cert_expiration.InSeconds());

  // The issuer is the same as the subject, since this cert is self-signed.
  X509_NAME* name = X509_get_subject_name(cert.get());
  if (!common_name.empty()) {
    X509_NAME_add_entry_by_txt(
        name, "CN", MBSTRING_UTF8,
        reinterpret_cast<const unsigned char*>(common_name.c_str()),
        common_name.length(), -1, 0);
  }
  X509_set_issuer_name(cert.get(), name);
  return cert;
}

std::unique_ptr<RSA, void(*)(RSA*)> GenerateRSAKeyPair(int key_length_bits) {
  // Create RSA key pair.
  auto rsa_key_pair = std::unique_ptr<RSA, void(*)(RSA*)>{RSA_new(), RSA_free};
  CHECK(rsa_key_pair.get());
  auto big_num = std::unique_ptr<BIGNUM, void(*)(BIGNUM*)>{BN_new(), BN_free};
  CHECK(big_num.get());
  CHECK(BN_set_word(big_num.get(), 65537));
  CHECK(RSA_generate_key_ex(rsa_key_pair.get(), key_length_bits, big_num.get(),
                            nullptr));
  return rsa_key_pair;
}

chromeos::SecureBlob StroreRSAPrivateKey(RSA* rsa_key_pair) {
  auto bio =
      std::unique_ptr<BIO, void(*)(BIO*)>{BIO_new(BIO_s_mem()), BIO_vfree};
  CHECK(bio);
  CHECK(PEM_write_bio_RSAPrivateKey(bio.get(), rsa_key_pair, nullptr, nullptr,
                                    0, nullptr, nullptr));
  uint8_t* buffer = nullptr;
  size_t size = BIO_get_mem_data(bio.get(), &buffer);
  CHECK_GT(size, 0u);
  CHECK(buffer);
  chromeos::SecureBlob key_blob(buffer, size);
  chromeos::SecureMemset(buffer, 0, size);
  return key_blob;
}

chromeos::Blob StroreCertificate(X509* cert) {
  auto bio =
      std::unique_ptr<BIO, void(*)(BIO*)>{BIO_new(BIO_s_mem()), BIO_vfree};
  CHECK(bio);
  CHECK(PEM_write_bio_X509(bio.get(), cert));
  uint8_t* buffer = nullptr;
  size_t size = BIO_get_mem_data(bio.get(), &buffer);
  CHECK_GT(size, 0u);
  CHECK(buffer);
  return chromeos::Blob(buffer, buffer + size);
}

// Same as openssl x509 -fingerprint -sha256.
chromeos::Blob GetSha256Fingerprint(X509* cert) {
  chromeos::Blob fingerpring(kSha256OutputSize);
  uint32_t len = 0;
  CHECK(X509_digest(cert, EVP_sha256(), fingerpring.data(), &len));
  CHECK_EQ(len, kSha256OutputSize);
  VLOG(3) << "Certificate fingerprint: " << base::HexEncode(fingerpring.data(),
                                                            fingerpring.size());
  return fingerpring;
}

}  // namespace

SecurityManager::SecurityManager() : secret_(kSha256OutputSize) {
  base::RandBytes(secret_.data(), kSha256OutputSize);
}

// Returns "base64([hmac]scope:time)".
std::string SecurityManager::CreateAccessToken(AuthScope scope,
                                               const base::Time& time) const {
  chromeos::SecureBlob data(CreateTokenData(scope, time));
  chromeos::Blob hash(HmacSha256(secret_, data));
  return Base64Encode(chromeos::SecureBlob::Combine(
      chromeos::SecureBlob(hash.begin(), hash.end()), data));
}

// Parses "base64([hmac]scope:time)".
privetd::AuthScope SecurityManager::ParseAccessToken(const std::string& token,
                                                     base::Time* time) const {
  chromeos::Blob decoded = Base64Decode(token);
  if (decoded.size() <= kSha256OutputSize)
    return AuthScope::kNone;
  chromeos::SecureBlob data(decoded.begin() + kSha256OutputSize, decoded.end());
  decoded.resize(kSha256OutputSize);
  if (decoded != HmacSha256(secret_, data))
    return AuthScope::kNone;
  return SplitTokenData(data.to_string(), time);
}

std::vector<PairingType> SecurityManager::GetPairingTypes() const {
  return {PairingType::kEmbeddedCode};
}

bool SecurityManager::IsValidPairingCode(const std::string& auth_code) const {
  return auth_code == GetPairingSecret();
}

privetd::Error SecurityManager::StartPairing(PairingType mode,
                                             std::string* session_id,
                                             std::string* device_commitment) {
  *session_id = session_id_;
  *device_commitment = device_commitment_;
  return Error::kNone;
}

privetd::Error SecurityManager::ConfirmPairing(
    const std::string& sessionId,
    const std::string& client_commitment,
    std::string* fingerprint,
    std::string* signature) {
  if (sessionId != session_id_)
    return Error::kUnknownSession;
  CHECK(!TLS_certificate_fingerprint_.empty());
  client_commitment_ = client_commitment;
  *fingerprint = Base64Encode(TLS_certificate_fingerprint_);
  chromeos::Blob cert_hmac =
      HmacSha256(chromeos::SecureBlob(GetPairingSecret()),
                 TLS_certificate_fingerprint_);
  *signature = Base64Encode(cert_hmac);
  return Error::kNone;
}

void SecurityManager::InitTlsData() {
  CHECK(TLS_certificate_.empty() && TLS_private_key_.empty());

  // TODO(avakulenko): verify these constants and provide sensible values
  // for the long-term.
  const int kKeyLengthBits = 1024;
  const base::TimeDelta kCertExpiration = base::TimeDelta::FromDays(365);
  const char kCommonName[] = "Chrome OS Core device";

  // Create the X509 certificate.
  int cert_serial_number = base::RandInt(0, std::numeric_limits<int>::max());
  auto cert =
      CreateCertificate(cert_serial_number, kCertExpiration, kCommonName);

  // Create RSA key pair.
  auto rsa_key_pair = GenerateRSAKeyPair(kKeyLengthBits);

  // Store the private key to a temp buffer.
  // Do not assign it to |TLS_private_key_| yet until the end when we are sure
  // everything else has worked out.
  chromeos::SecureBlob private_key = StroreRSAPrivateKey(rsa_key_pair.get());

  // Create EVP key and set it to the certificate.
  auto key = std::unique_ptr<EVP_PKEY, void (*)(EVP_PKEY*)>{EVP_PKEY_new(),
                                                            EVP_PKEY_free};
  CHECK(key.get());
  // Transfer ownership of |rsa_key_pair| to |key|.
  CHECK(EVP_PKEY_assign_RSA(key.get(), rsa_key_pair.release()));
  CHECK(X509_set_pubkey(cert.get(), key.get()));

  // Sign the certificate.
  CHECK(X509_sign(cert.get(), key.get(), EVP_sha256()));

  TLS_certificate_ = StroreCertificate(cert.get());
  TLS_certificate_fingerprint_ = GetSha256Fingerprint(cert.get());
  TLS_private_key_ = std::move(private_key);
}

const chromeos::SecureBlob& SecurityManager::GetTlsPrivateKey() const {
  CHECK(!TLS_private_key_.empty()) << "InitTlsData must be called first";
  return TLS_private_key_;
}

const chromeos::Blob& SecurityManager::GetTlsCertificate() const {
  CHECK(!TLS_certificate_.empty()) << "InitTlsData must be called first";
  return TLS_certificate_;
}

}  // namespace privetd
