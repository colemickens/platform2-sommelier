// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TPM_MANAGER_CLIENT_TPM_MANAGER_UTILITY_H_
#define TPM_MANAGER_CLIENT_TPM_MANAGER_UTILITY_H_

#include <base/macros.h>
#include <base/threading/thread.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "tpm_manager/client/tpm_nvram_dbus_proxy.h"
#include "tpm_manager/client/tpm_ownership_dbus_proxy.h"
#include "tpm_manager/common/export.h"

namespace tpm_manager {

// A TpmUtility implementation for version-independent functions.
class TPM_MANAGER_EXPORT TpmManagerUtility {
 public:
  TpmManagerUtility() = default;
  // a constructor which enables injection of mock interfaces.
  TpmManagerUtility(tpm_manager::TpmOwnershipInterface* tpm_owner,
                    tpm_manager::TpmNvramInterface* tpm_nvram);
  ~TpmManagerUtility() = default;

  // Initializes the worker thread and proxies of |tpm_manager| and returns
  // |true| if successful. Returns |false| if we cannot start
  // |tpm_manager_thread_| or tpm_manager's interfaces fail to initialize.
  bool Initialize();

  // Blocking call of |TpmOwnershipDBusProxy::TakeOwnership|. Returns |true| iff
  // the operation succeeds.
  bool TakeOwnership();

  // Blocking call of |TpmOwnershipDBusProxy::GetTpmStatus|.
  // Returns |true| iff the operation succeeds. Once returning |true|,
  // |is_enabled| indicates if TPM is enabled, and |is_owned| indicates if TPM
  // is owned. |local_data| is the current |LocalData| stored in the
  // |tpm_manager| service.
  bool GetTpmStatus(bool* is_enabled, bool* is_owned, LocalData* local_data);

  // Blocking call of
  // |TpmOwnershipDBusProxy::RemoveOwnerDependency|. Returns |true| iff the
  // operation succeeds. |dependency| is the idenitier of the dependency.
  bool RemoveOwnerDependency(const std::string& dependency);

  // Blocking call of |TpmOwnershipDBusProxy::GetDictionaryAttackInfo|. Returns
  // |true| iff the operation succeeds. Once returning |true|, |counter|,
  // |threshold|, |lockout| and |seconds_remaining| will set to the respective
  // values of received |GetDictionaryAttackInfoReply|.
  bool GetDictionaryAttackInfo(int* counter,
                               int* threshold,
                               bool* lockout,
                               int* seconds_remaining);

  // Blocking call of |TpmOwnershipDBusProxy::GetDictionaryAttackInfo|. Returns
  // |true| iff the operation succeeds.
  bool ResetDictionaryAttackLock();

  // Blocking call of |TpmOwnershipDBusProxy::ReadSpace|. Returns |true| iff
  // the operation succeeds. This call sends a request to read the content of
  // the nvram at |index| and stores the output data in |output|. If
  // |use_owner_auth| is set, the request tells the service to use owner
  // authorization. Note: currently the arbitrary auth value is not supported
  // since we got no use case for now.
  bool ReadSpace(uint32_t index, bool use_owner_auth, std::string* output);

 private:
  // Tpm_manager communication thread class that cleans up after stopping.
  class TpmManagerThread : public base::Thread {
   public:
    explicit TpmManagerThread(TpmManagerUtility* utility)
        : base::Thread("tpm_manager_thread"), utility_(utility) {
      DCHECK(utility_);
    }
    ~TpmManagerThread() override { Stop(); }

   private:
    void CleanUp() override { utility_->ShutdownTask(); }

    TpmManagerUtility* const utility_;

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

  DISALLOW_COPY_AND_ASSIGN(TpmManagerUtility);
};

}  // namespace tpm_manager

#endif  // TPM_MANAGER_CLIENT_TPM_MANAGER_UTILITY_H_
