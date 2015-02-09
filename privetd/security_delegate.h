// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRIVETD_SECURITY_DELEGATE_H_
#define PRIVETD_SECURITY_DELEGATE_H_

#include <memory>
#include <string>
#include <vector>

#include <base/time/time.h>
#include <chromeos/secure_blob.h>

#include "privetd/privet_types.h"

namespace privetd {

enum class PairingType {
  kPinCode,
  kEmbeddedCode,
  kUltrasoundDsssBroadcaster,
  kAudibleDtmfBroadcaster
};

// Scopes in order of increasing privileges.
enum class AuthScope {
  kNone,
  kGuest,
  kViewer,
  kUser,
  kOwner,
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

  // Creates access token for the given |scope| and |time|.
  virtual std::string CreateAccessToken(AuthScope scope,
                                        const base::Time& time) const = 0;

  // Validates |token| and returns |scope| for it.
  virtual AuthScope ParseAccessToken(const std::string& token,
                                     base::Time* time) const = 0;

  // Returns list of pairing methods by device.
  virtual std::vector<PairingType> GetPairingTypes() const = 0;

  // Returns list of crypto methods supported by devices.
  virtual std::vector<CryptoType> GetCryptoTypes() const = 0;

  // Returns true if |auth_code| provided by client is valid. Client should
  // obtain |auth_code| during pairing process.
  virtual bool IsValidPairingCode(const std::string& auth_code) const = 0;

  virtual Error StartPairing(PairingType mode,
                             CryptoType crypto,
                             std::string* session_id,
                             std::string* device_commitment) = 0;

  virtual Error ConfirmPairing(const std::string& sessionId,
                               const std::string& client_commitment,
                               std::string* fingerprint,
                               std::string* signature) = 0;
};

}  // namespace privetd

#endif  // PRIVETD_SECURITY_DELEGATE_H_
