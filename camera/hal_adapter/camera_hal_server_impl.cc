/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hal_adapter/camera_hal_server_impl.h"

#include <dlfcn.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#include <deque>
#include <string>
#include <utility>
#include <vector>

#include <base/bind.h>
#include <base/bind_helpers.h>
#include <base/files/file_enumerator.h>
#include <base/files/file_util.h>
#include <base/files/scoped_file.h>
#include <base/logging.h>
#include <base/message_loop/message_loop.h>
#include <base/threading/thread_task_runner_handle.h>
#include <mojo/edk/embedder/embedder.h>
#include <mojo/edk/embedder/platform_channel_pair.h>
#include <mojo/edk/embedder/platform_channel_utils_posix.h>
#include <mojo/edk/embedder/platform_handle_vector.h>

#include "arc/common.h"
#include "hal_adapter/ipc_util.h"

namespace arc {

namespace {

const base::FilePath kArcCamera3SocketPath("/run/camera/camera3.sock");
const base::FilePath kCameraHalDirs[] = {
    base::FilePath("/usr/lib/camera_hal"),
    base::FilePath("/usr/lib64/camera_hal")};

}  // namespace

CameraHalServerImpl::CameraHalServerImpl()
    : ipc_thread_("IPCThread"),
      main_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      binding_(this) {
  VLOGF_ENTER();
}

CameraHalServerImpl::~CameraHalServerImpl() {
  VLOGF_ENTER();
  mojo::edk::ShutdownIPCSupport();
}

bool CameraHalServerImpl::Start() {
  VLOGF_ENTER();
  mojo::edk::Init();
  if (!ipc_thread_.StartWithOptions(
          base::Thread::Options(base::MessageLoop::TYPE_IO, 0))) {
    LOGF(ERROR) << "Failed to start IPCThread";
    return false;
  }
  mojo::edk::InitIPCSupport(this, ipc_thread_.task_runner());

  watcher_.Watch(kArcCamera3SocketPath, false,
                 base::Bind(&CameraHalServerImpl::OnSocketFileStatusChange,
                            base::Unretained(this)));
  if (base::PathExists(kArcCamera3SocketPath)) {
    CameraHalServerImpl::OnSocketFileStatusChange(kArcCamera3SocketPath, false);
  }
  return true;
}

void CameraHalServerImpl::CreateChannel(
    mojom::CameraModuleRequest camera_module_request) {
  VLOGF_ENTER();
  DCHECK(ipc_thread_.task_runner()->BelongsToCurrentThread());
  camera_hal_adapter_->OpenCameraHal(std::move(camera_module_request));
}

void CameraHalServerImpl::OnShutdownComplete() {}

void CameraHalServerImpl::OnSocketFileStatusChange(
    const base::FilePath& socket_path,
    bool error) {
  VLOGF_ENTER();
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (!PathExists(socket_path)) {
    if (dispatcher_.is_bound()) {
      main_task_runner_->PostTask(
          FROM_HERE, base::Bind(&CameraHalServerImpl::ExitOnMainThread,
                                base::Unretained(this), ECONNRESET));
    }
    return;
  }

  if (dispatcher_.is_bound()) {
    return;
  }

  VLOG(1) << "Got socket: " << socket_path.value() << " error: " << error;
  base::ScopedFD client_socket_fd =
      internal::CreateClientUnixDomainSocket(socket_path);
  if (!client_socket_fd.is_valid()) {
    LOGF(WARNING) << "Failed to connect to " << socket_path.value();
    return;
  }
  mojo::edk::ScopedPlatformHandle socketHandle(
      mojo::edk::PlatformHandle(client_socket_fd.release()));

  // Set socket to blocking
  int flags = HANDLE_EINTR(fcntl(socketHandle.get().handle, F_GETFL));
  if (flags == -1) {
    PLOGF(ERROR) << "fcntl(F_GETFL) failed:";
    return;
  }
  if (HANDLE_EINTR(fcntl(socketHandle.get().handle, F_SETFL,
                         flags & ~O_NONBLOCK)) == -1) {
    PLOGF(ERROR) << "fcntl(F_SETFL) failed:";
    return;
  }

  const int kTokenSize = 32;
  char token[kTokenSize] = {};
  std::deque<mojo::edk::PlatformHandle> platformHandles;
  mojo::edk::PlatformChannelRecvmsg(socketHandle.get(), token, sizeof(token),
                                    &platformHandles, true);
  if (platformHandles.size() != 1) {
    LOGF(ERROR) << "Unexpected number of handles received, expected 1: "
                << platformHandles.size();
    return;
  }
  mojo::edk::ScopedPlatformHandle parent_pipe(platformHandles.back());
  platformHandles.pop_back();
  if (!parent_pipe.is_valid()) {
    LOGF(ERROR) << "Invalid parent pipe";
    return;
  }
  mojo::edk::SetParentPipeHandle(std::move(parent_pipe));

  mojo::ScopedMessagePipeHandle child_pipe =
      mojo::edk::CreateChildMessagePipe(std::string(token, kTokenSize));

  dispatcher_ = mojo::MakeProxy(
      mojom::CameraHalDispatcherPtrInfo(std::move(child_pipe), 0u),
      ipc_thread_.task_runner());
  LOGF(INFO) << "Connected to CameraHalDispatcher";
  ipc_thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&CameraHalServerImpl::RegisterCameraHal,
                            base::Unretained(this)));
}

void CameraHalServerImpl::RegisterCameraHal() {
  VLOGF_ENTER();
  DCHECK(ipc_thread_.task_runner()->BelongsToCurrentThread());

  std::vector<camera_module_t*> camera_modules;

  for (base::FilePath dir : kCameraHalDirs) {
    base::FileEnumerator dlls(dir, false, base::FileEnumerator::FILES, "*.so");
    for (base::FilePath dll = dlls.Next(); !dll.empty(); dll = dlls.Next()) {
      LOGF(INFO) << "Try to load camera hal " << dll.value();

      void* handle = dlopen(dll.value().c_str(), RTLD_NOW | RTLD_LOCAL);
      if (!handle) {
        LOGF(INFO) << "Failed to dlopen: " << dlerror();
        main_task_runner_->PostTask(
            FROM_HERE, base::Bind(&CameraHalServerImpl::ExitOnMainThread,
                                  base::Unretained(this), ENOENT));
        return;
      }

      auto* module = static_cast<camera_module_t*>(
          dlsym(handle, HAL_MODULE_INFO_SYM_AS_STR));
      if (!module) {
        LOGF(ERROR) << "Failed to get camera_module_t pointer with symbol name "
                    << HAL_MODULE_INFO_SYM_AS_STR << " from " << dll.value();
        main_task_runner_->PostTask(
            FROM_HERE, base::Bind(&CameraHalServerImpl::ExitOnMainThread,
                                  base::Unretained(this), ELIBBAD));
        return;
      }

      camera_modules.push_back(module);
    }
  }

  camera_hal_adapter_.reset(new CameraHalAdapter(camera_modules));
  LOGF(INFO) << "Running camera HAL adapter on " << getpid();

  if (!camera_hal_adapter_->Start()) {
    LOGF(ERROR) << "Failed to start camera HAL adapter";
    main_task_runner_->PostTask(
        FROM_HERE, base::Bind(&CameraHalServerImpl::ExitOnMainThread,
                              base::Unretained(this), ENODEV));
    return;
  }

  dispatcher_.set_connection_error_handler(base::Bind(
      &CameraHalServerImpl::OnServiceMojoChannelError, base::Unretained(this)));
  dispatcher_->RegisterServer(binding_.CreateInterfacePtrAndBind());
  LOGF(INFO) << "Registered camera HAL";
}

void CameraHalServerImpl::OnServiceMojoChannelError() {
  VLOGF_ENTER();
  DCHECK(ipc_thread_.task_runner()->BelongsToCurrentThread());
  // The ArcCamear3Service Mojo parent is probably dead. We need to restart
  // another process in order to connect to the new Mojo parent.
  LOGF(INFO) << "Mojo connection to ArcCamera3Service is broken";
  main_task_runner_->PostTask(FROM_HERE,
                              base::Bind(&CameraHalServerImpl::ExitOnMainThread,
                                         base::Unretained(this), ECONNRESET));
}

void CameraHalServerImpl::ExitOnMainThread(int exit_status) {
  VLOGF_ENTER();
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  exit(exit_status);
}

}  // namespace arc
