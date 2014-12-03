// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRIVETD_SECURITY_MANAGER_H_
#define PRIVETD_SECURITY_MANAGER_H_

#include <string>
#include <vector>

#include <chromeos/secure_blob.h>

#include "privetd/security_delegate.h"

namespace privetd {

class SecurityManager : public SecurityDelegate {
 public:
  SecurityManager();
  ~SecurityManager() override = default;

  // SecurityDelegate methods
  std::string CreateAccessToken(AuthScope scope,
                                const base::Time& time) const override;
  AuthScope ParseAccessToken(const std::string& token,
                             base::Time* time) const override;
  std::vector<PairingType> GetPairingTypes() const override;
  bool IsValidPairingCode(const std::string& auth_code) const override;
  Error StartPairing(PairingType mode,
                     std::string* session_id,
                     std::string* device_commitment) override;
  Error ConfirmPairing(const std::string& sessionId,
                       const std::string& client_commitment,
                       std::string* fingerprint,
                       std::string* signature) override;

  void InitTlsData();
  const chromeos::SecureBlob& GetTlsPrivateKey() const;
  const chromeos::Blob& GetTlsCertificate() const;

 private:
  std::string GetPairingSecret() const {
    return device_commitment_ + client_commitment_;
  }

  std::string session_id_ = "111";
  std::string client_commitment_;
  std::string device_commitment_ = "1234";
  chromeos::SecureBlob secret_;
  chromeos::Blob TLS_certificate_;
  chromeos::Blob TLS_certificate_fingerprint_;
  chromeos::SecureBlob TLS_private_key_;

  DISALLOW_COPY_AND_ASSIGN(SecurityManager);
};

}  // namespace privetd

#endif  // PRIVETD_SECURITY_MANAGER_H_
