// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_SRC_PRIVET_SECURITY_MANAGER_H_
#define LIBWEAVE_SRC_PRIVET_SECURITY_MANAGER_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <base/callback.h>
#include <base/files/file_path.h>
#include <base/memory/weak_ptr.h>
#include <chromeos/errors/error.h>
#include <chromeos/secure_blob.h>
#include <weave/task_runner.h>

#include "libweave/src/privet/security_delegate.h"

namespace crypto {
class P224EncryptedKeyExchange;
}  // namespace crypto

namespace weave {
namespace privet {

class SecurityManager : public SecurityDelegate {
 public:
  using PairingStartListener =
      base::Callback<void(const std::string& session_id,
                          PairingType pairing_type,
                          const std::vector<uint8_t>& code)>;
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

  SecurityManager(const std::set<PairingType>& pairing_modes,
                  const base::FilePath& embedded_code_path,
                  TaskRunner* task_runner,
                  bool disable_security);
  ~SecurityManager() override;

  // SecurityDelegate methods
  std::string CreateAccessToken(const UserInfo& user_info,
                                const base::Time& time) override;
  UserInfo ParseAccessToken(const std::string& token,
                            base::Time* time) const override;
  std::set<PairingType> GetPairingTypes() const override;
  std::set<CryptoType> GetCryptoTypes() const override;
  bool IsValidPairingCode(const std::string& auth_code) const override;

  bool StartPairing(PairingType mode,
                    CryptoType crypto,
                    std::string* session_id,
                    std::string* device_commitment,
                    chromeos::ErrorPtr* error) override;

  bool ConfirmPairing(const std::string& session_id,
                      const std::string& client_commitment,
                      std::string* fingerprint,
                      std::string* signature,
                      chromeos::ErrorPtr* error) override;
  bool CancelPairing(const std::string& session_id,
                     chromeos::ErrorPtr* error) override;

  void RegisterPairingListeners(const PairingStartListener& on_start,
                                const PairingEndListener& on_end);

  void SetCertificateFingerprint(const chromeos::Blob& fingerprint) {
    certificate_fingerprint_ = fingerprint;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(SecurityManagerTest, ThrottlePairing);
  // Allows limited number of new sessions without successful authorization.
  bool CheckIfPairingAllowed(chromeos::ErrorPtr* error);
  bool ClosePendingSession(const std::string& session_id);
  bool CloseConfirmedSession(const std::string& session_id);

  // If true allows unencrypted pairing and accepts any access code.
  bool is_security_disabled_{false};
  std::set<PairingType> pairing_modes_;
  const base::FilePath embedded_code_path_;
  std::string embedded_code_;
  // TODO(vitalybuka): Session cleanup can be done without posting tasks.
  TaskRunner* task_runner_{nullptr};
  std::map<std::string, std::unique_ptr<KeyExchanger>> pending_sessions_;
  std::map<std::string, std::unique_ptr<KeyExchanger>> confirmed_sessions_;
  mutable int pairing_attemts_{0};
  mutable base::Time block_pairing_until_;
  chromeos::SecureBlob secret_;
  chromeos::Blob certificate_fingerprint_;
  PairingStartListener on_start_;
  PairingEndListener on_end_;

  base::WeakPtrFactory<SecurityManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SecurityManager);
};

}  // namespace privet
}  // namespace weave

#endif  // LIBWEAVE_SRC_PRIVET_SECURITY_MANAGER_H_
