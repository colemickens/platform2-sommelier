/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common/camera_algorithm_bridge_impl.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

#include "base/bind.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/platform_channel_utils_posix.h"
#include "mojo/edk/embedder/platform_handle_vector.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"

#include "arc/common.h"
#include "common/camera_algorithm_internal.h"

namespace arc {

// static
CameraAlgorithmBridge* CameraAlgorithmBridge::GetInstance() {
  static CameraAlgorithmBridgeImpl bridge_impl;
  return &bridge_impl;
}

CameraAlgorithmBridgeImpl::CameraAlgorithmBridgeImpl()
    : ipc_thread_("IPC thread") {}

CameraAlgorithmBridgeImpl::~CameraAlgorithmBridgeImpl() {
  VLOGF_ENTER();
  ipc_thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&CameraAlgorithmBridgeImpl::DestroyOnIpcThread,
                            base::Unretained(this)));
  ipc_thread_.Stop();
  VLOGF_EXIT();
}

int32_t CameraAlgorithmBridgeImpl::Initialize(
    const camera_algorithm_callback_ops_t* callback_ops) {
  VLOGF_ENTER();
  if (!ipc_thread_.StartWithOptions(
          base::Thread::Options(base::MessageLoop::TYPE_IO, 0))) {
    LOGF(ERROR) << "Failed to start IPC thread";
    return -EFAULT;
  }
  auto future = internal::Future<int32_t>::Create(&relay_);
  ipc_thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&CameraAlgorithmBridgeImpl::InitializeOnIpcThread,
                            base::Unretained(this), callback_ops,
                            internal::GetFutureCallback(future)));
  future->Wait();
  VLOGF_EXIT();
  return future->Get();
}

int32_t CameraAlgorithmBridgeImpl::RegisterBuffer(int buffer_fd) {
  VLOGF_ENTER();
  auto future = internal::Future<int32_t>::Create(&relay_);
  ipc_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&CameraAlgorithmBridgeImpl::RegisterBufferOnIpcThread,
                 base::Unretained(this), buffer_fd,
                 internal::GetFutureCallback(future)));
  future->Wait();
  VLOGF_EXIT();
  return future->Get();
}

int32_t CameraAlgorithmBridgeImpl::Request(
    const std::vector<uint8_t>& req_header,
    int32_t buffer_handle) {
  VLOGF_ENTER();
  auto future = internal::Future<int32_t>::Create(&relay_);
  ipc_thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&CameraAlgorithmBridgeImpl::RequestOnIpcThread,
                            base::Unretained(this), req_header, buffer_handle,
                            internal::GetFutureCallback(future)));
  future->Wait();
  VLOGF_EXIT();
  return future->Get();
}

void CameraAlgorithmBridgeImpl::DeregisterBuffers(
    const std::vector<int32_t>& buffer_handles) {
  VLOGF_ENTER();
  auto future = internal::Future<void>::Create(&relay_);
  ipc_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&CameraAlgorithmBridgeImpl::DeregisterBuffersOnIpcThread,
                 base::Unretained(this), buffer_handles,
                 internal::GetFutureCallback(future)));
  future->Wait();
  VLOGF_EXIT();
}

void CameraAlgorithmBridgeImpl::InitializeOnIpcThread(
    const camera_algorithm_callback_ops_t* callback_ops,
    base::Callback<void(int32_t)> cb) {
  DCHECK(ipc_thread_.task_runner()->BelongsToCurrentThread());
  VLOGF_ENTER();
  if (!callback_ops || !callback_ops->return_callback) {
    cb.Run(-EINVAL);
    return;
  }
  if (cb_impl_) {
    LOGF(ERROR) << "Camera algorithm bridge is already initialized";
    cb.Run(-EALREADY);
    return;
  }

  // Creat unix socket to send the adapter token and connection handle
  int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) {
    LOGF(ERROR) << "Failed to create communication socket";
    cb.Run(-1);
    return;
  }
  base::ScopedFD socket_fd(fd);
  struct sockaddr_un srv_addr = {};
  srv_addr.sun_family = AF_UNIX;
  strcpy(srv_addr.sun_path, kArcCameraAlgoSocketPath);
  if (HANDLE_EINTR(connect(socket_fd.get(), (struct sockaddr*)&srv_addr,
                           strlen(kArcCameraAlgoSocketPath) +
                               offsetof(struct sockaddr_un, sun_path))) == -1) {
    LOGF(ERROR) << "Failed to connect to the server";
    cb.Run(-1);
    return;
  }

  VLOGF(1) << "Setting up message pipe";
  mojo::edk::PlatformChannelPair channel_pair;
  mojo::edk::Init();
  mojo::edk::InitIPCSupport(this, ipc_thread_.task_runner());
  const int kUnusedProcessHandle = 0;
  mojo::edk::ChildProcessLaunched(kUnusedProcessHandle,
                                  channel_pair.PassServerHandle());
  mojo::edk::ScopedPlatformHandleVectorPtr handles(
      new mojo::edk::PlatformHandleVector(
          {channel_pair.PassClientHandle().release()}));
  std::string token = mojo::edk::GenerateRandomToken();
  VLOGF(1) << "Generated token: " << token;
  mojo::ScopedMessagePipeHandle parent_pipe =
      mojo::edk::CreateParentMessagePipe(token);
  struct iovec iov = {const_cast<char*>(token.c_str()), token.length()};
  if (mojo::edk::PlatformChannelSendmsgWithHandles(
          mojo::edk::PlatformHandle(socket_fd.get()), &iov, 1, handles->data(),
          handles->size()) == -1) {
    LOGF(ERROR) << "Failed to send token and handle: " << strerror(errno);
    cb.Run(-1);
    return;
  }
  interface_ptr_.Bind(
      mojom::CameraAlgorithmOpsPtrInfo(std::move(parent_pipe), 0u));
  interface_ptr_.set_connection_error_handler(base::Bind(
      &CameraAlgorithmBridgeImpl::DestroyOnIpcThread, base::Unretained(this)));
  cb_impl_.reset(new CameraAlgorithmCallbackOpsImpl(ipc_thread_.task_runner(),
                                                    callback_ops));
  interface_ptr_->Initialize(cb_impl_->CreateInterfacePtr(),
                             mojo::Callback<void(int32_t)>(cb));
  VLOGF_EXIT();
}

void CameraAlgorithmBridgeImpl::DestroyOnIpcThread() {
  DCHECK(ipc_thread_.task_runner()->BelongsToCurrentThread());
  VLOGF_ENTER();
  cb_impl_.reset();
  interface_ptr_.reset();
  mojo::edk::ShutdownIPCSupport();
  VLOGF_EXIT();
}

void CameraAlgorithmBridgeImpl::RegisterBufferOnIpcThread(
    int buffer_fd,
    base::Callback<void(int32_t)> cb) {
  DCHECK(ipc_thread_.task_runner()->BelongsToCurrentThread());
  VLOGF_ENTER();
  int dup_fd = dup(buffer_fd);
  if (dup_fd < 0) {
    PLOGF(ERROR) << "Failed to dup fd: ";
    cb.Run(-errno);
    return;
  }
  MojoHandle wrapped_handle;
  MojoResult wrap_result = mojo::edk::CreatePlatformHandleWrapper(
      mojo::edk::ScopedPlatformHandle(mojo::edk::PlatformHandle(dup_fd)),
      &wrapped_handle);
  if (wrap_result != MOJO_RESULT_OK) {
    cb.Run(-EBADF);
    return;
  }
  interface_ptr_->RegisterBuffer(
      mojo::ScopedHandle(mojo::Handle(wrapped_handle)),
      mojo::Callback<void(int32_t)>(cb));
}

void CameraAlgorithmBridgeImpl::RequestOnIpcThread(
    const std::vector<uint8_t>& req_header,
    int32_t buffer_handle,
    base::Callback<void(int32_t)> cb) {
  DCHECK(ipc_thread_.task_runner()->BelongsToCurrentThread());
  VLOGF_ENTER();
  interface_ptr_->Request(mojo::Array<uint8_t>::From(req_header), buffer_handle,
                          mojo::Callback<void(int32_t)>(cb));
  VLOGF_EXIT();
}

void CameraAlgorithmBridgeImpl::DeregisterBuffersOnIpcThread(
    const std::vector<int32_t>& buffer_handles,
    base::Callback<void(void)> cb) {
  DCHECK(ipc_thread_.task_runner()->BelongsToCurrentThread());
  VLOGF_ENTER();
  interface_ptr_->DeregisterBuffers(mojo::Array<int32_t>::From(buffer_handles));
  cb.Run();
}

}  // namespace arc
