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

#include <base/bind.h>
#include <base/guid.h>
#include <base/logging.h>
#include <base/message_loop/message_loop.h>
#include <base/rand_util.h>
#include <base/stl_util.h>
#include <base/strings/string_number_conversions.h>
#include <base/time/time.h>
#include <chromeos/strings/string_utils.h>
#include <crypto/p224_spake.h>

#include "privetd/constants.h"
#include "privetd/openssl_utils.h"

namespace privetd {

namespace {

const char kTokenDelimeter = ':';
const int kSessionExpirationTimeMinutes = 5;
const int kPairingExpirationTimeMinutes = 5;

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

class Spakep224Exchanger : public SecurityManager::KeyExchanger {
 public:
  explicit Spakep224Exchanger(const std::string& password)
      : spake_(crypto::P224EncryptedKeyExchange::kPeerTypeServer, password) {}
  ~Spakep224Exchanger() override = default;

  // SecurityManager::KeyExchanger methods.
  const std::string& GetMessage() override { return spake_.GetMessage(); }

  bool ProcessMessage(const std::string& message,
                      chromeos::ErrorPtr* error) override {
    switch (spake_.ProcessMessage(message)) {
      case crypto::P224EncryptedKeyExchange::kResultPending:
        return true;
      case crypto::P224EncryptedKeyExchange::kResultFailed:
        chromeos::Error::AddTo(error, FROM_HERE, errors::kPrivetdErrorDomain,
                               errors::kInvalidClientCommitment,
                               spake_.error());
        return false;
      default:
        LOG(FATAL) << "SecurityManager uses only one round trip";
    }
    return false;
  }

  const std::string& GetKey() const override {
    return spake_.GetUnverifiedKey();
  }

 private:
  crypto::P224EncryptedKeyExchange spake_;
};

class UnsecureKeyExchanger : public SecurityManager::KeyExchanger {
 public:
  explicit UnsecureKeyExchanger(const std::string& password)
      : password_(password) {}
  ~UnsecureKeyExchanger() override = default;

  // SecurityManager::KeyExchanger methods.
  const std::string& GetMessage() override { return password_; }

  bool ProcessMessage(const std::string& message,
                      chromeos::ErrorPtr* error) override {
    if (password_ == message)
      return true;
    chromeos::Error::AddTo(error, FROM_HERE, errors::kPrivetdErrorDomain,
                           errors::kInvalidClientCommitment,
                           "Commitment does not match the pairing code.");
    return false;
  }

  const std::string& GetKey() const override { return password_; }

 private:
  std::string password_;
};

}  // namespace

SecurityManager::SecurityManager(const std::string& embedded_password,
                                 bool disable_security)
    : is_security_disabled_(disable_security),
      embedded_password_(embedded_password),
      secret_(kSha256OutputSize) {
  base::RandBytes(secret_.data(), kSha256OutputSize);
}

SecurityManager::~SecurityManager() {
  while (!pending_sessions_.empty())
    ClosePendingSession(pending_sessions_.begin()->first);
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
AuthScope SecurityManager::ParseAccessToken(const std::string& token,
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

std::vector<CryptoType> SecurityManager::GetCryptoTypes() const {
  std::vector<CryptoType> result{CryptoType::kSpake_p224};
  if (is_security_disabled_)
    result.push_back(CryptoType::kNone);
  return result;
}

bool SecurityManager::IsValidPairingCode(const std::string& auth_code) const {
  if (is_security_disabled_)
    return true;
  chromeos::Blob auth_decoded = Base64Decode(auth_code);
  for (const auto& session : confirmed_sessions_) {
    if (auth_decoded ==
        HmacSha256(chromeos::SecureBlob{session.second->GetKey()},
                   chromeos::SecureBlob{session.first})) {
      return true;
    }
  }
  return false;
}

Error SecurityManager::StartPairing(PairingType mode,
                                    CryptoType crypto,
                                    std::string* session_id,
                                    std::string* device_commitment) {
  if (mode != PairingType::kEmbeddedCode)
    return Error::kInvalidParams;

  // TODO(vitalybuka) Implement throttling of StartPairing to avoid
  //                  brute force attacks.
  std::unique_ptr<KeyExchanger> spake;
  switch (crypto) {
    case CryptoType::kNone:
      if (!is_security_disabled_)
        return Error::kInvalidParams;
      spake.reset(new UnsecureKeyExchanger(embedded_password_));
      break;
    case CryptoType::kSpake_p224:
      spake.reset(new Spakep224Exchanger(embedded_password_));
      break;
    default:
      return Error::kInvalidParams;
  }

  // Allow only a single session at a time for now.
  while (!pending_sessions_.empty())
    ClosePendingSession(pending_sessions_.begin()->first);

  std::string session;
  do {
    session = base::GenerateGUID();
  } while (confirmed_sessions_.find(session) != confirmed_sessions_.end() ||
           pending_sessions_.find(session) != pending_sessions_.end());
  std::string commitment = spake->GetMessage();
  pending_sessions_.emplace(session, std::move(spake));

  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE, base::Bind(&SecurityManager::ClosePendingSession,
                            weak_ptr_factory_.GetWeakPtr(), session),
      base::TimeDelta::FromMinutes(kPairingExpirationTimeMinutes));

  *session_id = session;
  *device_commitment = Base64Encode(chromeos::SecureBlob(commitment));
  if (!on_start_.is_null())
    on_start_.Run(session, mode, embedded_password_);

  return Error::kNone;
}

Error SecurityManager::ConfirmPairing(const std::string& session_id,
                                      const std::string& client_commitment,
                                      std::string* fingerprint,
                                      std::string* signature) {
  auto session = pending_sessions_.find(session_id);
  if (session == pending_sessions_.end())
    return Error::kUnknownSession;
  CHECK(!TLS_certificate_fingerprint_.empty());

  chromeos::ErrorPtr error;
  chromeos::Blob commitment{Base64Decode(client_commitment)};
  if (!session->second->ProcessMessage(
          std::string(commitment.begin(), commitment.end()), &error)) {
    LOG(ERROR) << "Confirmation failed: " << error->GetMessage();
    ClosePendingSession(session_id);
    return Error::kCommitmentMismatch;
  }

  std::string key = session->second->GetKey();
  VLOG(3) << "KEY " << base::HexEncode(key.data(), key.size());

  *fingerprint = Base64Encode(TLS_certificate_fingerprint_);
  chromeos::Blob cert_hmac =
      HmacSha256(chromeos::SecureBlob(session->second->GetKey()),
                 TLS_certificate_fingerprint_);
  *signature = Base64Encode(cert_hmac);
  confirmed_sessions_.emplace(session->first, std::move(session->second));
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE, base::Bind(&SecurityManager::CloseConfirmedSession,
                            weak_ptr_factory_.GetWeakPtr(), session_id),
      base::TimeDelta::FromMinutes(kSessionExpirationTimeMinutes));
  ClosePendingSession(session_id);
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

void SecurityManager::RegisterPairingListeners(
    const PairingStartListener& on_start,
    const PairingEndListener& on_end) {
  CHECK(on_start_.is_null() && on_end_.is_null());
  on_start_ = on_start;
  on_end_  = on_end;
}

void SecurityManager::ClosePendingSession(const std::string& session_id) {
  const size_t num_erased = pending_sessions_.erase(session_id);
  if (num_erased > 0 && !on_end_.is_null())
    on_end_.Run(session_id);
}

void SecurityManager::CloseConfirmedSession(const std::string& session_id) {
  confirmed_sessions_.erase(session_id);
}

}  // namespace privetd
