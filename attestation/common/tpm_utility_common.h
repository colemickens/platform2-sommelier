// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATTESTATION_COMMON_TPM_UTILITY_COMMON_H_
#define ATTESTATION_COMMON_TPM_UTILITY_COMMON_H_

#include <stdint.h>

#include "attestation/common/tpm_utility.h"

#include <map>
#include <memory>
#include <string>

#include <base/macros.h>
#include <base/threading/thread.h>
#include <crypto/scoped_openssl_types.h>

#include "tpm_manager/client/tpm_nvram_dbus_proxy.h"
#include "tpm_manager/client/tpm_ownership_dbus_proxy.h"

namespace attestation {

// A TpmUtility implementation for version-independent functions.
class TpmUtilityCommon : public TpmUtility {
 public:
  TpmUtilityCommon() = default;
  TpmUtilityCommon(tpm_manager::TpmOwnershipInterface* tpm_owner,
                   tpm_manager::TpmNvramInterface* tpm_nvram);
  ~TpmUtilityCommon() override;

  // TpmUtility methods.
  bool Initialize() override;
  bool IsTpmReady() override;
  bool RemoveOwnerDependency() override;

 protected:
  // Tpm_manager communication thread class that cleans up after stopping.
  class TpmManagerThread : public base::Thread {
   public:
    explicit TpmManagerThread(TpmUtilityCommon* tpm_utility)
        : base::Thread("tpm_manager_thread"), tpm_utility_(tpm_utility) {
      DCHECK(tpm_utility_);
    }
    ~TpmManagerThread() override {
      Stop();
    }

   private:
    void CleanUp() override {
      tpm_utility_->ShutdownTask();
    }

    TpmUtilityCommon* const tpm_utility_;

    DISALLOW_COPY_AND_ASSIGN(TpmManagerThread);
  };

  // Initialization operation that must be performed on the tpm_manager
  // thread.
  void InitializationTask(base::WaitableEvent* completion);

  // Shutdown operation that must be performed on the tpm_manager thread.
  void ShutdownTask();

  // Sends a request to tpm_managerd and waits for a response. The given
  // interface |method| will be called and a |reply_proto| will be populated.
  //
  // Example usage:
  //
  // tpm_manager::GetTpmStatusReply tpm_status;
  // SendTpmManagerRequestAndWait(
  //     base::Bind(&tpm_manager::TpmOwnershipInterface::GetTpmStatus,
  //                base::Unretained(tpm_owner_),
  //                tpm_manager::GetTpmStatusRequest()),
  //     &tpm_status);
  template <typename ReplyProtoType, typename MethodType>
  void SendTpmManagerRequestAndWait(const MethodType& method,
                                    ReplyProtoType* reply_proto);

  // Gets the endorsement password from tpm_managerd. Returns false if the
  // password is not available.
  bool GetEndorsementPassword(std::string* password);

  // Gets the owner password from tpm_managerd. Returns false if the password is
  // not available.
  bool GetOwnerPassword(std::string* password);

  // Caches various TPM state including owner / endorsement passwords. On
  // success, fields like is_ready_ and owner_password_ will be populated.
  // Returns true on success.
  bool CacheTpmState();

  static crypto::ScopedRSA CreateRSAFromRawModulus(uint8_t* modulus_buffer,
                                                   size_t modulus_size);

  bool is_ready_{false};
  std::string endorsement_password_;
  std::string owner_password_;
  std::string delegate_blob_;
  std::string delegate_secret_;
  // |tpm_owner_| and |tpm_nvram_| typically point to |default_tpm_owner_| and
  // |default_tpm_nvram_| respectively, created/destroyed on the
  // |tpm_manager_thread_|. As such, should not be accessed after that thread
  // is stopped/destroyed.
  tpm_manager::TpmOwnershipInterface* tpm_owner_{nullptr};
  tpm_manager::TpmNvramInterface* tpm_nvram_{nullptr};
  // |default_tpm_owner_| and |default_tpm_nvram_| are created and destroyed
  // on the |tpm_manager_thread_|, and are not available after the thread is
  // stopped/destroyed.
  std::unique_ptr<tpm_manager::TpmOwnershipDBusProxy> default_tpm_owner_;
  std::unique_ptr<tpm_manager::TpmNvramDBusProxy> default_tpm_nvram_;

  // A message loop thread dedicated for asynchronous communication with
  // tpm_managerd. Declared last, so that it is destroyed before the
  // objects it uses.
  TpmManagerThread tpm_manager_thread_{this};

  DISALLOW_COPY_AND_ASSIGN(TpmUtilityCommon);
};

}  // namespace attestation

#endif  // ATTESTATION_COMMON_TPM_UTILITY_COMMON_H_
