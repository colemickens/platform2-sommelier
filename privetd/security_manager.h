// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRIVETD_SECURITY_MANAGER_H_
#define PRIVETD_SECURITY_MANAGER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <base/callback.h>
#include <base/memory/weak_ptr.h>
#include <chromeos/errors/error.h>
#include <chromeos/secure_blob.h>

#include "privetd/security_delegate.h"

namespace crypto {
class P224EncryptedKeyExchange;
}  // namespace crypto

namespace privetd {

class SecurityManager : public SecurityDelegate {
 public:
  using PairingStartListener =
      base::Callback<void(const std::string& session_id,
                          PairingType pairing_type,
                          const std::string& code)>;
  using PairingEndListener =
      base::Callback<void(const std::string& session_id)>;

  class KeyExchanger {
   public:
    virtual ~KeyExchanger() = default;

    virtual const std::string& GetMessage() = 0;
    virtual bool ProcessMessage(const std::string& message,
                                chromeos::ErrorPtr* error) = 0;
    virtual const std::string& GetKey() const = 0;
  };

  explicit SecurityManager(const std::string& embedded_code,
                           bool disable_security = false);
  ~SecurityManager() override;

  // SecurityDelegate methods
  std::string CreateAccessToken(AuthScope scope,
                                const base::Time& time) const override;
  AuthScope ParseAccessToken(const std::string& token,
                             base::Time* time) const override;
  std::vector<PairingType> GetPairingTypes() const override;
  std::vector<CryptoType> GetCryptoTypes() const override;
  bool IsValidPairingCode(const std::string& auth_code) const override;
  Error StartPairing(PairingType mode,
                     CryptoType crypto,
                     std::string* session_id,
                     std::string* device_commitment) override;

  Error ConfirmPairing(const std::string& session_id,
                       const std::string& client_commitment,
                       std::string* fingerprint,
                       std::string* signature) override;
  void RegisterPairingListeners(const PairingStartListener& on_start,
                                const PairingEndListener& on_end);

  void InitTlsData();
  const chromeos::SecureBlob& GetTlsPrivateKey() const;
  const chromeos::Blob& GetTlsCertificate() const;

 private:
  void ClosePendingSession(const std::string& session_id);
  void CloseConfirmedSession(const std::string& session_id);

  // If true allows unencrypted pairing and accepts any access code.
  bool is_security_disabled_{false};
  const std::string embedded_code_;
  std::map<std::string, std::unique_ptr<KeyExchanger>> pending_sessions_;
  std::map<std::string, std::unique_ptr<KeyExchanger>> confirmed_sessions_;
  chromeos::SecureBlob secret_;
  chromeos::Blob TLS_certificate_;
  chromeos::Blob TLS_certificate_fingerprint_;
  chromeos::SecureBlob TLS_private_key_;
  PairingStartListener on_start_;
  PairingEndListener on_end_;

  base::WeakPtrFactory<SecurityManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SecurityManager);
};

}  // namespace privetd

#endif  // PRIVETD_SECURITY_MANAGER_H_
