// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libweave/src/privet/security_manager.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <set>

#include <base/bind.h>
#include <base/guid.h>
#include <base/logging.h>
#include <base/rand_util.h>
#include <base/stl_util.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/stringprintf.h>
#include <base/time/time.h>
#include <chromeos/key_value_store.h>
#include <weave/task_runner.h>

#include "libweave/external/crypto/p224_spake.h"
#include "libweave/src/data_encoding.h"
#include "libweave/src/privet/constants.h"
#include "libweave/src/privet/openssl_utils.h"
#include "libweave/src/string_utils.h"

namespace weave {
namespace privet {

namespace {

const char kTokenDelimeter[] = ":";
const int kSessionExpirationTimeMinutes = 5;
const int kPairingExpirationTimeMinutes = 5;
const int kMaxAllowedPairingAttemts = 3;
const int kPairingBlockingTimeMinutes = 1;

const char kEmbeddedCode[] = "embedded_code";

// Returns "scope:id:time".
std::string CreateTokenData(const UserInfo& user_info, const base::Time& time) {
  return base::IntToString(static_cast<int>(user_info.scope())) +
         kTokenDelimeter + base::Uint64ToString(user_info.user_id()) +
         kTokenDelimeter + base::Int64ToString(time.ToTimeT());
}

// Splits string of "scope:id:time" format.
UserInfo SplitTokenData(const std::string& token, base::Time* time) {
  const UserInfo kNone;
  auto parts = Split(token, kTokenDelimeter, false, false);
  if (parts.size() != 3)
    return kNone;
  int scope = 0;
  if (!base::StringToInt(parts[0], &scope) ||
      scope < static_cast<int>(AuthScope::kNone) ||
      scope > static_cast<int>(AuthScope::kOwner)) {
    return kNone;
  }

  uint64_t id{0};
  if (!base::StringToUint64(parts[1], &id))
    return kNone;

  int64_t timestamp{0};
  if (!base::StringToInt64(parts[2], &timestamp))
    return kNone;
  *time = base::Time::FromTimeT(timestamp);
  return UserInfo{static_cast<AuthScope>(scope), id};
}

std::string LoadEmbeddedCode(const base::FilePath& path) {
  std::string code;
  chromeos::KeyValueStore store;
  if (store.Load(path))
    store.GetString(kEmbeddedCode, &code);
  return code;
}

class Spakep224Exchanger : public SecurityManager::KeyExchanger {
 public:
  explicit Spakep224Exchanger(const std::string& password)
      : spake_(crypto::P224EncryptedKeyExchange::kPeerTypeServer, password) {}
  ~Spakep224Exchanger() override = default;

  // SecurityManager::KeyExchanger methods.
  const std::string& GetMessage() override { return spake_.GetNextMessage(); }

  bool ProcessMessage(const std::string& message, ErrorPtr* error) override {
    switch (spake_.ProcessMessage(message)) {
      case crypto::P224EncryptedKeyExchange::kResultPending:
        return true;
      case crypto::P224EncryptedKeyExchange::kResultFailed:
        Error::AddTo(error, FROM_HERE, errors::kDomain,
                     errors::kInvalidClientCommitment, spake_.error());
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

  bool ProcessMessage(const std::string& message, ErrorPtr* error) override {
    return true;
  }

  const std::string& GetKey() const override { return password_; }

 private:
  std::string password_;
};

}  // namespace

SecurityManager::SecurityManager(const std::set<PairingType>& pairing_modes,
                                 const base::FilePath& embedded_code_path,
                                 TaskRunner* task_runner,
                                 bool disable_security)
    : is_security_disabled_(disable_security),
      pairing_modes_(pairing_modes),
      embedded_code_path_(embedded_code_path),
      task_runner_{task_runner},
      secret_(kSha256OutputSize) {
  base::RandBytes(secret_.data(), kSha256OutputSize);

  CHECK_EQ(embedded_code_path_.empty(),
           std::find(pairing_modes_.begin(), pairing_modes_.end(),
                     PairingType::kEmbeddedCode) == pairing_modes_.end());
}

SecurityManager::~SecurityManager() {
  while (!pending_sessions_.empty())
    ClosePendingSession(pending_sessions_.begin()->first);
}

// Returns "base64([hmac]scope:id:time)".
std::string SecurityManager::CreateAccessToken(const UserInfo& user_info,
                                               const base::Time& time) {
  std::string data_str{CreateTokenData(user_info, time)};
  std::vector<uint8_t> data{data_str.begin(), data_str.end()};
  std::vector<uint8_t> hash{HmacSha256(secret_, data)};
  hash.insert(hash.end(), data.begin(), data.end());
  return Base64Encode(hash);
}

// Parses "base64([hmac]scope:id:time)".
UserInfo SecurityManager::ParseAccessToken(const std::string& token,
                                           base::Time* time) const {
  std::vector<uint8_t> decoded;
  if (!Base64Decode(token, &decoded) || decoded.size() <= kSha256OutputSize) {
    return UserInfo{};
  }
  std::vector<uint8_t> data(decoded.begin() + kSha256OutputSize, decoded.end());
  decoded.resize(kSha256OutputSize);
  if (decoded != HmacSha256(secret_, data))
    return UserInfo{};
  return SplitTokenData(std::string(data.begin(), data.end()), time);
}

std::set<PairingType> SecurityManager::GetPairingTypes() const {
  return pairing_modes_;
}

std::set<CryptoType> SecurityManager::GetCryptoTypes() const {
  std::set<CryptoType> result{CryptoType::kSpake_p224};
  if (is_security_disabled_)
    result.insert(CryptoType::kNone);
  return result;
}

bool SecurityManager::IsValidPairingCode(const std::string& auth_code) const {
  if (is_security_disabled_)
    return true;
  std::vector<uint8_t> auth_decoded;
  if (!Base64Decode(auth_code, &auth_decoded))
    return false;
  for (const auto& session : confirmed_sessions_) {
    const std::string& key = session.second->GetKey();
    const std::string& id = session.first;
    if (auth_decoded ==
        HmacSha256(std::vector<uint8_t>(key.begin(), key.end()),
                   std::vector<uint8_t>(id.begin(), id.end()))) {
      pairing_attemts_ = 0;
      block_pairing_until_ = base::Time{};
      return true;
    }
  }
  LOG(ERROR) << "Attempt to authenticate with invalide code.";
  return false;
}

bool SecurityManager::StartPairing(PairingType mode,
                                   CryptoType crypto,
                                   std::string* session_id,
                                   std::string* device_commitment,
                                   ErrorPtr* error) {
  if (!CheckIfPairingAllowed(error))
    return false;

  if (std::find(pairing_modes_.begin(), pairing_modes_.end(), mode) ==
      pairing_modes_.end()) {
    Error::AddTo(error, FROM_HERE, errors::kDomain, errors::kInvalidParams,
                 "Pairing mode is not enabled");
    return false;
  }

  std::string code;
  switch (mode) {
    case PairingType::kEmbeddedCode:
      CHECK(!embedded_code_path_.empty());

      if (embedded_code_.empty())
        embedded_code_ = LoadEmbeddedCode(embedded_code_path_);

      if (embedded_code_.empty()) {  // File is not created yet.
        Error::AddTo(error, FROM_HERE, errors::kDomain, errors::kDeviceBusy,
                     "Embedded code is not ready");
        return false;
      }

      code = embedded_code_;
      break;
    case PairingType::kUltrasound32:
    case PairingType::kAudible32: {
      code = base::RandBytesAsString(4);
      break;
    }
    case PairingType::kPinCode:
      code = base::StringPrintf("%04i", base::RandInt(0, 9999));
      break;
    default:
      Error::AddTo(error, FROM_HERE, errors::kDomain, errors::kInvalidParams,
                   "Unsupported pairing mode");
      return false;
  }

  std::unique_ptr<KeyExchanger> spake;
  switch (crypto) {
    case CryptoType::kSpake_p224:
      spake.reset(new Spakep224Exchanger(code));
      break;
    case CryptoType::kNone:
      if (is_security_disabled_) {
        spake.reset(new UnsecureKeyExchanger(code));
        break;
      }
    // Fall through...
    default:
      Error::AddTo(error, FROM_HERE, errors::kDomain, errors::kInvalidParams,
                   "Unsupported crypto");
      return false;
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

  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::Bind(base::IgnoreResult(&SecurityManager::ClosePendingSession),
                 weak_ptr_factory_.GetWeakPtr(), session),
      base::TimeDelta::FromMinutes(kPairingExpirationTimeMinutes));

  *session_id = session;
  *device_commitment = Base64Encode(commitment);
  LOG(INFO) << "Pairing code for session " << *session_id << " is " << code;
  // TODO(vitalybuka): Handle case when device can't start multiple pairing
  // simultaneously and implement throttling to avoid brute force attack.
  if (!on_start_.is_null()) {
    on_start_.Run(session, mode,
                  std::vector<uint8_t>{code.begin(), code.end()});
  }

  return true;
}

bool SecurityManager::ConfirmPairing(const std::string& session_id,
                                     const std::string& client_commitment,
                                     std::string* fingerprint,
                                     std::string* signature,
                                     ErrorPtr* error) {
  auto session = pending_sessions_.find(session_id);
  if (session == pending_sessions_.end()) {
    Error::AddToPrintf(error, FROM_HERE, errors::kDomain,
                       errors::kUnknownSession, "Unknown session id: '%s'",
                       session_id.c_str());
    return false;
  }
  CHECK(!certificate_fingerprint_.empty());

  std::vector<uint8_t> commitment;
  if (!Base64Decode(client_commitment, &commitment)) {
    ClosePendingSession(session_id);
    Error::AddToPrintf(
        error, FROM_HERE, errors::kDomain, errors::kInvalidFormat,
        "Invalid commitment string: '%s'", client_commitment.c_str());
    return false;
  }

  if (!session->second->ProcessMessage(
          std::string(commitment.begin(), commitment.end()), error)) {
    ClosePendingSession(session_id);
    Error::AddTo(error, FROM_HERE, errors::kDomain, errors::kCommitmentMismatch,
                 "Pairing code or crypto implementation mismatch");
    return false;
  }

  const std::string& key = session->second->GetKey();
  VLOG(3) << "KEY " << base::HexEncode(key.data(), key.size());

  *fingerprint = Base64Encode(certificate_fingerprint_);
  std::vector<uint8_t> cert_hmac = HmacSha256(
      std::vector<uint8_t>(key.begin(), key.end()), certificate_fingerprint_);
  *signature = Base64Encode(cert_hmac);
  confirmed_sessions_.emplace(session->first, std::move(session->second));
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::Bind(base::IgnoreResult(&SecurityManager::CloseConfirmedSession),
                 weak_ptr_factory_.GetWeakPtr(), session_id),
      base::TimeDelta::FromMinutes(kSessionExpirationTimeMinutes));
  ClosePendingSession(session_id);
  return true;
}

bool SecurityManager::CancelPairing(const std::string& session_id,
                                    ErrorPtr* error) {
  bool confirmed = CloseConfirmedSession(session_id);
  bool pending = ClosePendingSession(session_id);
  if (pending) {
    CHECK_GE(pairing_attemts_, 1);
    --pairing_attemts_;
  }
  CHECK(!confirmed || !pending);
  if (confirmed || pending)
    return true;
  Error::AddToPrintf(error, FROM_HERE, errors::kDomain, errors::kUnknownSession,
                     "Unknown session id: '%s'", session_id.c_str());
  return false;
}

void SecurityManager::RegisterPairingListeners(
    const PairingStartListener& on_start,
    const PairingEndListener& on_end) {
  CHECK(on_start_.is_null() && on_end_.is_null());
  on_start_ = on_start;
  on_end_ = on_end;
}

bool SecurityManager::CheckIfPairingAllowed(ErrorPtr* error) {
  if (is_security_disabled_)
    return true;

  if (block_pairing_until_ > base::Time::Now()) {
    Error::AddTo(error, FROM_HERE, errors::kDomain, errors::kDeviceBusy,
                 "Too many pairing attempts");
    return false;
  }

  if (++pairing_attemts_ >= kMaxAllowedPairingAttemts) {
    LOG(INFO) << "Pairing blocked for" << kPairingBlockingTimeMinutes
              << "minutes.";
    block_pairing_until_ = base::Time::Now();
    block_pairing_until_ +=
        base::TimeDelta::FromMinutes(kPairingBlockingTimeMinutes);
  }

  return true;
}

bool SecurityManager::ClosePendingSession(const std::string& session_id) {
  // The most common source of these session_id values is the map containing
  // the sessions, which we're about to clear out.  Make a local copy.
  const std::string safe_session_id{session_id};
  const size_t num_erased = pending_sessions_.erase(safe_session_id);
  if (num_erased > 0 && !on_end_.is_null())
    on_end_.Run(safe_session_id);
  return num_erased != 0;
}

bool SecurityManager::CloseConfirmedSession(const std::string& session_id) {
  return confirmed_sessions_.erase(session_id) != 0;
}

}  // namespace privet
}  // namespace weave
