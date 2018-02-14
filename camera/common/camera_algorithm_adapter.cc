/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common/camera_algorithm_adapter.h"

#include <dlfcn.h>

#include <string>
#include <utility>

#include <base/bind.h>
#include <mojo/edk/embedder/embedder.h>
#include <mojo/public/cpp/bindings/interface_request.h>

#include "cros-camera/camera_algorithm.h"
#include "cros-camera/common.h"

namespace cros {

CameraAlgorithmAdapter::CameraAlgorithmAdapter()
    : algo_impl_(CameraAlgorithmOpsImpl::GetInstance()),
      algo_dll_handle_(nullptr),
      ipc_thread_("IPC thread") {}

void CameraAlgorithmAdapter::Run(
    std::string mojo_token,
    mojo::edk::ScopedPlatformHandle channel_handle) {
  VLOGF_ENTER();
  auto future = cros::Future<void>::Create(&relay_);
  ipc_lost_cb_ = cros::GetFutureCallback(future);
  ipc_thread_.StartWithOptions(
      base::Thread::Options(base::MessageLoop::TYPE_IO, 0));
  ipc_thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&CameraAlgorithmAdapter::InitializeOnIpcThread,
                            base::Unretained(this), mojo_token,
                            base::Passed(&channel_handle)));
  future->Wait(-1);
  ipc_thread_.Stop();
  VLOGF_EXIT();
}

void CameraAlgorithmAdapter::InitializeOnIpcThread(
    std::string mojo_token,
    mojo::edk::ScopedPlatformHandle channel_handle) {
  DCHECK(ipc_thread_.task_runner()->BelongsToCurrentThread());
  VLOGF(1) << "Setting up message pipe";
  mojo::edk::Init();
  mojo::edk::InitIPCSupport(this, ipc_thread_.task_runner());
  mojo::edk::SetParentPipeHandle(std::move(channel_handle));
  mojo::ScopedMessagePipeHandle child_pipe =
      mojo::edk::CreateChildMessagePipe(mojo_token);
  mojom::CameraAlgorithmOpsRequest request;
  request.Bind(std::move(child_pipe));

  const char kCameraAlgorithmDllName[] = "libcam_algo.so";
  VLOGF_ENTER();
  algo_dll_handle_ = dlopen(kCameraAlgorithmDllName, RTLD_NOW);
  if (!algo_dll_handle_) {
    LOGF(ERROR) << "Failed to dlopen: " << dlerror();
    DestroyOnIpcThread();
    return;
  }
  camera_algorithm_ops_t* cam_algo = static_cast<camera_algorithm_ops_t*>(
      dlsym(algo_dll_handle_, CAMERA_ALGORITHM_MODULE_INFO_SYM_AS_STR));
  if (!cam_algo) {
    LOGF(ERROR) << "Camera algorithm is invalid";
    dlclose(algo_dll_handle_);
    DestroyOnIpcThread();
    return;
  }

  base::Closure ipc_lost_handler = base::Bind(
      &CameraAlgorithmAdapter::DestroyOnIpcThread, base::Unretained(this));
  algo_impl_->Bind(std::move(request), cam_algo, ipc_thread_.task_runner(),
                   ipc_lost_handler);
  VLOGF_EXIT();
}

void CameraAlgorithmAdapter::DestroyOnIpcThread() {
  DCHECK(ipc_thread_.task_runner()->BelongsToCurrentThread());
  VLOGF_ENTER();
  algo_impl_->Unbind();
  mojo::edk::ShutdownIPCSupport();
  if (algo_dll_handle_) {
    dlclose(algo_dll_handle_);
  }
  ipc_lost_cb_.Run();
  VLOGF_EXIT();
}

}  // namespace cros
