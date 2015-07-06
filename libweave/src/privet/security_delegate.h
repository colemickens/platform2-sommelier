// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_SRC_PRIVET_SECURITY_DELEGATE_H_
#define LIBWEAVE_SRC_PRIVET_SECURITY_DELEGATE_H_

#include <memory>
#include <set>
#include <string>

#include <base/time/time.h>
#include <chromeos/secure_blob.h>

#include "libweave/src/privet/privet_types.h"

namespace privetd {

enum class PairingType {
  kPinCode,
  kEmbeddedCode,
  kUltrasound32,
  kAudible32,
};

enum class CryptoType {
  kNone,
  kSpake_p224,
  kSpake_p256,
};

// Interface to provide Security related logic for |PrivetHandler|.
class SecurityDelegate {
 public:
  virtual ~SecurityDelegate() = default;

  // Creates access token for the given scope, user id and |time|.
  virtual std::string CreateAccessToken(const UserInfo& user_info,
                                        const base::Time& time) = 0;

  // Validates |token| and returns scope and user id parsed from that.
  virtual UserInfo ParseAccessToken(const std::string& token,
                                    base::Time* time) const = 0;

  // Returns list of pairing methods by device.
  virtual std::set<PairingType> GetPairingTypes() const = 0;

  // Returns list of crypto methods supported by devices.
  virtual std::set<CryptoType> GetCryptoTypes() const = 0;

  // Returns true if |auth_code| provided by client is valid. Client should
  // obtain |auth_code| during pairing process.
  virtual bool IsValidPairingCode(const std::string& auth_code) const = 0;

  virtual bool StartPairing(PairingType mode,
                            CryptoType crypto,
                            std::string* session_id,
                            std::string* device_commitment,
                            chromeos::ErrorPtr* error) = 0;

  virtual bool ConfirmPairing(const std::string& session_id,
                              const std::string& client_commitment,
                              std::string* fingerprint,
                              std::string* signature,
                              chromeos::ErrorPtr* error) = 0;

  virtual bool CancelPairing(const std::string& session_id,
                             chromeos::ErrorPtr* error) = 0;
};

bool StringToPairingType(const std::string& mode, PairingType* id);
std::string PairingTypeToString(PairingType id);

bool StringToAuthScope(const std::string& scope, AuthScope* id);
std::string AuthScopeToString(AuthScope id);

}  // namespace privetd

#endif  // LIBWEAVE_SRC_PRIVET_SECURITY_DELEGATE_H_
