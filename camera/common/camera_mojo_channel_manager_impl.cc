/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common/camera_mojo_channel_manager_impl.h"

#include <memory>
#include <utility>

#include <base/bind.h>
#include <base/logging.h>

#include "cros-camera/common.h"
#include "cros-camera/constants.h"
#include "cros-camera/ipc_util.h"

namespace cros {

// static
mojom::CameraHalDispatcherPtr CameraMojoChannelManagerImpl::dispatcher_;
base::Thread* CameraMojoChannelManagerImpl::ipc_thread_ = nullptr;
base::Lock CameraMojoChannelManagerImpl::static_lock_;
int CameraMojoChannelManagerImpl::reference_count_ = 0;
MojoShutdownImpl* CameraMojoChannelManagerImpl::mojo_shutdown_impl_ = nullptr;

// static
std::unique_ptr<CameraMojoChannelManager>
CameraMojoChannelManager::CreateInstance() {
  return base::WrapUnique<CameraMojoChannelManager>(
      new CameraMojoChannelManagerImpl());
}

CameraMojoChannelManagerImpl::CameraMojoChannelManagerImpl() {
  VLOGF_ENTER();
  bool success = Start();
  DCHECK(success);
}

CameraMojoChannelManagerImpl::~CameraMojoChannelManagerImpl() {
  base::AutoLock l(static_lock_);
  VLOGF_ENTER();

  reference_count_--;

  ipc_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&CameraMojoChannelManagerImpl::DestroyOnIpcThreadLocked,
                 base::Unretained(this)));

  if (reference_count_ == 0) {
    ipc_thread_->Stop();
    delete ipc_thread_;
    delete mojo_shutdown_impl_;
    ipc_thread_ = nullptr;
    mojo_shutdown_impl_ = nullptr;
  }

  VLOGF_EXIT();
}

void CameraMojoChannelManagerImpl::DestroyOnIpcThreadLocked() {
  DCHECK(ipc_thread_->task_runner()->BelongsToCurrentThread());

  // There is not any mojo users.
  // We enter this function only from destructor. The lock is used in the
  // destructor.
  if (reference_count_ == 0) {
    dispatcher_.reset();
    mojo::edk::ShutdownIPCSupport();
  }

  VLOGF_EXIT();
}

bool CameraMojoChannelManagerImpl::Start() {
  base::AutoLock l(static_lock_);

  reference_count_++;
  if (reference_count_ > 1) {
    return true;
  }

  ipc_thread_ = new base::Thread("MojoIpcThread");
  mojo::edk::Init();
  if (!ipc_thread_->StartWithOptions(
          base::Thread::Options(base::MessageLoop::TYPE_IO, 0))) {
    LOGF(ERROR) << "Failed to start IPC Thread";
    delete ipc_thread_;
    ipc_thread_ = nullptr;
    reference_count_--;
    return false;
  }
  mojo_shutdown_impl_ = new MojoShutdownImpl();
  mojo::edk::InitIPCSupport(mojo_shutdown_impl_, ipc_thread_->task_runner());

  return true;
}

void CameraMojoChannelManagerImpl::CreateJpegDecodeAccelerator(
    mojom::JpegDecodeAcceleratorRequest request) {
  ipc_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(
          &CameraMojoChannelManagerImpl::CreateJpegDecodeAcceleratorOnIpcThread,
          base::Unretained(this), base::Passed(std::move(request))));
}

void CameraMojoChannelManagerImpl::CreateJpegEncodeAccelerator(
    mojom::JpegEncodeAcceleratorRequest request) {
  ipc_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(
          &CameraMojoChannelManagerImpl::CreateJpegEncodeAcceleratorOnIpcThread,
          base::Unretained(this), base::Passed(std::move(request))));
}

mojom::CameraAlgorithmOpsPtr
CameraMojoChannelManagerImpl::CreateCameraAlgorithmOpsPtr() {
  VLOGF_ENTER();

  mojo::ScopedMessagePipeHandle parent_pipe;
  mojom::CameraAlgorithmOpsPtr algorithm_ops;

  base::FilePath socket_path(constants::kCrosCameraAlgoSocketPathString);
  MojoResult result = cros::CreateMojoChannelToChildByUnixDomainSocket(
      socket_path, &parent_pipe);
  if (result != MOJO_RESULT_OK) {
    LOGF(WARNING) << "Failed to create Mojo Channel to" << socket_path.value();
    return nullptr;
  }

  algorithm_ops.Bind(
      mojom::CameraAlgorithmOpsPtrInfo(std::move(parent_pipe), 0u));

  LOGF(INFO) << "Connected to CameraAlgorithmOps";

  VLOGF_EXIT();
  return algorithm_ops;
}

void CameraMojoChannelManagerImpl::CreateJpegDecodeAcceleratorOnIpcThread(
    mojom::JpegDecodeAcceleratorRequest request) {
  DCHECK(ipc_thread_->task_runner()->BelongsToCurrentThread());

  EnsureDispatcherConnectedOnIpcThread();
  if (dispatcher_.is_bound()) {
    dispatcher_->GetJpegDecodeAccelerator(std::move(request));
  }
}

void CameraMojoChannelManagerImpl::CreateJpegEncodeAcceleratorOnIpcThread(
    mojom::JpegEncodeAcceleratorRequest request) {
  DCHECK(ipc_thread_->task_runner()->BelongsToCurrentThread());

  EnsureDispatcherConnectedOnIpcThread();
  if (dispatcher_.is_bound()) {
    dispatcher_->GetJpegEncodeAccelerator(std::move(request));
  }
}

void CameraMojoChannelManagerImpl::EnsureDispatcherConnectedOnIpcThread() {
  DCHECK(ipc_thread_->task_runner()->BelongsToCurrentThread());
  VLOGF_ENTER();

  if (dispatcher_.is_bound()) {
    return;
  }

  mojo::ScopedMessagePipeHandle child_pipe;

  base::FilePath socket_path(constants::kCrosCameraSocketPathString);
  MojoResult result = cros::CreateMojoChannelToParentByUnixDomainSocket(
      socket_path, &child_pipe);
  if (result != MOJO_RESULT_OK) {
    LOGF(WARNING) << "Failed to create Mojo Channel to" << socket_path.value();
    return;
  }

  dispatcher_ = mojo::MakeProxy(
      mojom::CameraHalDispatcherPtrInfo(std::move(child_pipe), 0u),
      ipc_thread_->task_runner());
  dispatcher_.set_connection_error_handler(
      base::Bind(&CameraMojoChannelManagerImpl::OnDispatcherError));

  LOGF(INFO) << "Connected to CameraHalDispatcher";

  VLOGF_EXIT();
}

// static
void CameraMojoChannelManagerImpl::OnDispatcherError() {
  DCHECK(ipc_thread_->task_runner()->BelongsToCurrentThread());
  VLOGF_ENTER();
  LOGF(ERROR) << "Mojo channel to CameraHalDispatcher is broken";
  dispatcher_.reset();
  VLOGF_EXIT();
}

}  // namespace cros
