// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "privetd/security_manager.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <vector>

#include <base/bind.h>
#include <base/guid.h>
#include <base/logging.h>
#include <base/message_loop/message_loop.h>
#include <base/rand_util.h>
#include <base/stl_util.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/stringprintf.h>
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

SecurityManager::SecurityManager(const std::string& embedded_code,
                                 bool disable_security)
    : is_security_disabled_(disable_security),
      embedded_code_(embedded_code),
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
  return {embedded_code_.empty() ? PairingType::kPinCode
                                 : PairingType::kEmbeddedCode};
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
  std::string code;
  switch (mode) {
    case PairingType::kEmbeddedCode:
      code = embedded_code_;
      break;
    case PairingType::kPinCode:
      code = base::StringPrintf("%04i", base::RandInt(0, 9999));
      break;
    default:
      return Error::kInvalidParams;
  }

  // TODO(vitalybuka) Implement throttling of StartPairing to avoid
  //                  brute force attacks.
  std::unique_ptr<KeyExchanger> spake;
  switch (crypto) {
    case CryptoType::kNone:
      if (!is_security_disabled_)
        return Error::kInvalidParams;
      spake.reset(new UnsecureKeyExchanger(code));
      break;
    case CryptoType::kSpake_p224:
      spake.reset(new Spakep224Exchanger(code));
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
      FROM_HERE,
      base::Bind(base::IgnoreResult(&SecurityManager::ClosePendingSession),
                 weak_ptr_factory_.GetWeakPtr(), session),
      base::TimeDelta::FromMinutes(kPairingExpirationTimeMinutes));

  *session_id = session;
  *device_commitment = Base64Encode(chromeos::SecureBlob(commitment));
  VLOG(3) << "Pairing code for session " << *session_id << " is " << code;
  // TODO(vitalybuka): Handle case when device can't start multiple pairing
  // simultaneously and implement throttling to avoid brute force attack.
  if (!on_start_.is_null())
    on_start_.Run(session, mode, code);

  return Error::kNone;
}

Error SecurityManager::ConfirmPairing(const std::string& session_id,
                                      const std::string& client_commitment,
                                      std::string* fingerprint,
                                      std::string* signature) {
  auto session = pending_sessions_.find(session_id);
  if (session == pending_sessions_.end())
    return Error::kUnknownSession;
  CHECK(!certificate_fingerprint_.empty());

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

  *fingerprint = Base64Encode(certificate_fingerprint_);
  chromeos::Blob cert_hmac =
      HmacSha256(chromeos::SecureBlob(session->second->GetKey()),
                 certificate_fingerprint_);
  *signature = Base64Encode(cert_hmac);
  confirmed_sessions_.emplace(session->first, std::move(session->second));
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(base::IgnoreResult(&SecurityManager::CloseConfirmedSession),
                 weak_ptr_factory_.GetWeakPtr(), session_id),
      base::TimeDelta::FromMinutes(kSessionExpirationTimeMinutes));
  ClosePendingSession(session_id);
  return Error::kNone;
}

Error SecurityManager::CancelPairing(const std::string& session_id) {
  bool confirmed = CloseConfirmedSession(session_id);
  bool pending = ClosePendingSession(session_id);
  CHECK(!confirmed || !pending);
  return (confirmed || pending) ? Error::kNone : Error::kUnknownSession;
}

void SecurityManager::RegisterPairingListeners(
    const PairingStartListener& on_start,
    const PairingEndListener& on_end) {
  CHECK(on_start_.is_null() && on_end_.is_null());
  on_start_ = on_start;
  on_end_  = on_end;
}

bool SecurityManager::ClosePendingSession(const std::string& session_id) {
  const size_t num_erased = pending_sessions_.erase(session_id);
  if (num_erased > 0 && !on_end_.is_null())
    on_end_.Run(session_id);
  return num_erased != 0;
}

bool SecurityManager::CloseConfirmedSession(const std::string& session_id) {
  return confirmed_sessions_.erase(session_id) != 0;
}

}  // namespace privetd
