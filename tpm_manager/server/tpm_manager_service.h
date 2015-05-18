// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TPM_MANAGER_SERVER_TPM_MANAGER_SERVICE_H_
#define TPM_MANAGER_SERVER_TPM_MANAGER_SERVICE_H_

#include "tpm_manager/common/tpm_manager_interface.h"

#include <memory>

#include <base/callback.h>
#include <base/macros.h>
#include <base/memory/weak_ptr.h>
#include <base/threading/thread.h>
#include <chromeos/bind_lambda.h>

namespace tpm_manager {

// This class implements the core tpm_manager service. All Tpm access is
// asynchronous, except for the initial setup in Initialize().
// Usage:
//   std::unique_ptr<TpmManagerInterface> tpm_manager = new TpmManagerService();
//   CHECK(tpm_manager->Initialize());
//   tpm_manager->GetTpmStatus(...);
//
// THREADING NOTES:
// This class runs a worker thread and delegates all calls to it. This keeps the
// public methods non-blocking while allowing complex implementation details
// with dependencies on the TPM, network, and filesystem to be coded in a more
// readable way. It also serves to serialize method execution which reduces
// complexity with TPM state.
//
// Tasks that run on the worker thread are bound with base::Unretained which is
// safe because the thread is owned by this class (so it is guaranteed not to
// process a task after destruction). Weak pointers are used to post replies
// back to the main thread.
class TpmManagerService : public TpmManagerInterface {
 public:
  TpmManagerService();
  ~TpmManagerService() override = default;

  // TpmManagerInterface methods.
  bool Initialize() override;
  void GetTpmStatus(const GetTpmStatusRequest& request,
                    const GetTpmStatusCallback& callback) override;

 private:
  // A relay callback which allows the use of weak pointer semantics for a reply
  // to TaskRunner::PostTaskAndReply.
  template<typename ReplyProtobufType>
  void TaskRelayCallback(
      const base::Callback<void(const ReplyProtobufType&)> callback,
      const std::shared_ptr<ReplyProtobufType>& reply) {
    callback.Run(*reply);
  }

  // Blocking implementation of GetTpmStatus that can be executed on the
  // background worker thread.
  void GetTpmStatusTask(const GetTpmStatusRequest& request,
                        const std::shared_ptr<GetTpmStatusReply>& result);

  // Background thread to allow processing of potentially large TPM
  // requests in the background.
  std::unique_ptr<base::Thread> worker_thread_;

  // Declared last so any weak pointers are destroyed first.
  base::WeakPtrFactory<TpmManagerService> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(TpmManagerService);
};

}  // namespace tpm_manager

#endif  // TPM_MANAGER_SERVER_TPM_MANAGER_SERVICE_H_
