// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRIVETD_SECURITY_DELEGATE_H_
#define PRIVETD_SECURITY_DELEGATE_H_

#include <string>
#include <vector>

#include <base/time/time.h>

namespace privetd {

enum class PairingType {
  kPinCode,
  kEmbeddedCode,
};

enum class AuthScope {
  kNone,
  kGuest,
  kViewer,
  kUser,
  kOwner,
};

// Interface to provide Security related logic for |PrivetHandler|.
class SecurityDelegate {
 public:
  SecurityDelegate();
  virtual ~SecurityDelegate();

  // Creates access token for the given |scope|.
  virtual std::string CreateAccessToken(AuthScope scope) const = 0;

  // Validates |token| and returns |scope| for it. Discards all tokens issued
  // before |expire_before| time.
  virtual AuthScope GetScopeFromAccessToken(
      const std::string& token,
      const base::Time& expire_before) const = 0;

  // Returns list of pairing methods by device.
  virtual std::vector<PairingType> GetPairingTypes() const = 0;

  // Returns true if |auth_code| provided by client is valid. Client should
  // obtain |auth_code| during pairing process.
  virtual bool IsValidPairingCode(const std::string& auth_code) const = 0;
};

}  // namespace privetd

#endif  // PRIVETD_SECURITY_DELEGATE_H_
