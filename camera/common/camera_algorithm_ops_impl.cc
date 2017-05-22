/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common/camera_algorithm_ops_impl.h"

#include "base/bind.h"
#include "base/logging.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"

#include "arc/common.h"
#include "arc/future.h"

namespace arc {

CameraAlgorithmOpsImpl* CameraAlgorithmOpsImpl::singleton_ = nullptr;

CameraAlgorithmOpsImpl::CameraAlgorithmOpsImpl()
    : binding_(this), cam_algo_(nullptr) {
  singleton_ = this;
}

// static
CameraAlgorithmOpsImpl* CameraAlgorithmOpsImpl::GetInstance() {
  static CameraAlgorithmOpsImpl impl;
  return &impl;
}

bool CameraAlgorithmOpsImpl::Bind(
    mojom::CameraAlgorithmOpsRequest request,
    camera_algorithm_ops_t* cam_algo,
    scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner,
    base::Closure& ipc_lost_handler) {
  DCHECK(ipc_task_runner->BelongsToCurrentThread());
  if (binding_.is_bound()) {
    LOGF(ERROR) << "Algorithm Ops is already bound";
    return false;
  }
  DCHECK(!cam_algo_);
  DCHECK(!ipc_task_runner_);
  DCHECK(!cb_ptr_.is_bound());
  binding_.Bind(std::move(request));
  cam_algo_ = cam_algo;
  ipc_task_runner_ = std::move(ipc_task_runner);
  binding_.set_connection_error_handler(ipc_lost_handler);
  return true;
}

void CameraAlgorithmOpsImpl::Unbind() {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  DCHECK(binding_.is_bound());
  DCHECK(cam_algo_);
  DCHECK(ipc_task_runner_);
  cb_ptr_.reset();
  ipc_task_runner_ = nullptr;
  cam_algo_ = nullptr;
  binding_.Unbind();
}

void CameraAlgorithmOpsImpl::Initialize(
    mojom::CameraAlgorithmCallbackOpsPtr callback_ops,
    const InitializeCallback& callback) {
  DCHECK(cam_algo_);
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  DCHECK(callback_ops.is_bound());
  VLOGF_ENTER();
  int32_t result = 0;
  if (cb_ptr_.is_bound()) {
    LOGF(ERROR) << "Return callback is already registered";
    callback.Run(-EINVAL);
    return;
  }
  CameraAlgorithmOpsImpl::return_callback =
      CameraAlgorithmOpsImpl::ReturnCallbackForwarder;
  result = cam_algo_->initialize(this);
  cb_ptr_ = std::move(callback_ops);
  callback.Run(result);
  VLOGF_EXIT();
}

void CameraAlgorithmOpsImpl::RegisterBuffer(
    mojo::ScopedHandle buffer_fd,
    const RegisterBufferCallback& callback) {
  DCHECK(cam_algo_);
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  VLOGF_ENTER();
  mojo::edk::ScopedPlatformHandle scoped_platform_handle;
  MojoResult mojo_result = mojo::edk::PassWrappedPlatformHandle(
      buffer_fd.release().value(), &scoped_platform_handle);
  if (mojo_result != MOJO_RESULT_OK) {
    LOGF(ERROR) << "Failed to unwrap handle: " << mojo_result;
    callback.Run(-EBADF);
    return;
  }
  int32_t result =
      cam_algo_->register_buffer(scoped_platform_handle.release().handle);
  callback.Run(result);
  VLOGF_EXIT();
}

void CameraAlgorithmOpsImpl::Request(mojo::Array<uint8_t> req_header,
                                     int32_t buffer_handle,
                                     const RequestCallback& callback) {
  DCHECK(cam_algo_);
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  VLOGF_ENTER();
  if (!cb_ptr_.is_bound()) {
    LOGF(ERROR) << "Return callback is not registered yet";
    callback.Run(-EINVAL);
    return;
  }
  int32_t result = cam_algo_->request(req_header.PassStorage().data(),
                                      req_header.size(), buffer_handle);
  callback.Run(result);
  VLOGF_EXIT();
}

void CameraAlgorithmOpsImpl::DeregisterBuffers(
    mojo::Array<int32_t> buffer_handles) {
  DCHECK(cam_algo_);
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  VLOGF_ENTER();
  cam_algo_->deregister_buffers(buffer_handles.PassStorage().data(),
                                buffer_handles.size());
  VLOGF_EXIT();
}

// static
int32_t CameraAlgorithmOpsImpl::ReturnCallbackForwarder(
    const camera_algorithm_callback_ops_t* callback_ops,
    int32_t buffer_handle) {
  VLOGF_ENTER();
  if (const_cast<CameraAlgorithmOpsImpl*>(
          static_cast<const CameraAlgorithmOpsImpl*>(callback_ops)) !=
      singleton_) {
    return -EINVAL;
  }
  auto future = internal::Future<int32_t>::Create(nullptr);
  singleton_->ipc_task_runner_->PostTask(
      FROM_HERE, base::Bind(&CameraAlgorithmOpsImpl::ReturnCallbackOnIPCThread,
                            base::Unretained(singleton_), buffer_handle,
                            internal::GetFutureCallback(future)));
  future->Wait();
  return future->Get();
}

void CameraAlgorithmOpsImpl::ReturnCallbackOnIPCThread(
    int32_t buffer_handle,
    base::Callback<void(int32_t)> cb) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  VLOGF_ENTER();
  if (!cb_ptr_.is_bound()) {
    cb.Run(-ENOENT);
  }
  cb_ptr_->Return(buffer_handle, cb);
  VLOGF_EXIT();
}

}  // namespace arc
