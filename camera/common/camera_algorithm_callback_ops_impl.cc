/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <utility>

#include "common/camera_algorithm_callback_ops_impl.h"

#include "arc/common.h"

namespace arc {

CameraAlgorithmCallbackOpsImpl::CameraAlgorithmCallbackOpsImpl(
    scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner,
    const camera_algorithm_callback_ops_t* callback_ops)
    : binding_(this),
      ipc_task_runner_(std::move(ipc_task_runner)),
      callback_ops_(callback_ops) {}

void CameraAlgorithmCallbackOpsImpl::Return(int32_t buffer_handle,
                                            const ReturnCallback& callback) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  DCHECK(callback_ops_);
  DCHECK(callback_ops_->return_callback);
  VLOGF_ENTER();
  callback.Run(callback_ops_->return_callback(callback_ops_, buffer_handle));
  VLOGF_EXIT();
}

mojom::CameraAlgorithmCallbackOpsPtr
CameraAlgorithmCallbackOpsImpl::CreateInterfacePtr() {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  return binding_.CreateInterfacePtrAndBind();
}

}  // namespace arc
