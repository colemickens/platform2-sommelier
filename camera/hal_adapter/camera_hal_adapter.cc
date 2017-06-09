/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hal_adapter/camera_hal_adapter.h"

#include <fcntl.h>

#include <deque>
#include <utility>

#include <base/bind.h>
#include <base/bind_helpers.h>
#include <base/logging.h>
#include <base/posix/eintr_wrapper.h>
#include <base/threading/thread_task_runner_handle.h>
#include <mojo/edk/embedder/embedder.h>
#include <mojo/edk/embedder/platform_channel_pair.h>
#include <mojo/edk/embedder/platform_channel_utils_posix.h>
#include <mojo/edk/embedder/platform_handle_vector.h>
#include <mojo/edk/embedder/scoped_platform_handle.h>

#include "arc/common.h"
#include "hal_adapter/arc_camera3_mojo_utils.h"
#include "hal_adapter/camera_device_adapter.h"
#include "hal_adapter/camera_module_callbacks_delegate.h"
#include "hal_adapter/camera_module_delegate.h"

namespace arc {

CameraHalAdapter::CameraHalAdapter(camera_module_t* camera_module,
                                   int socket_fd,
                                   base::Closure quit_cb)
    : camera_module_(camera_module),
      quit_cb_(std::move(quit_cb)),
      socket_fd_(socket_fd),
      ipc_thread_("MojoIpcThread"),
      camera_module_thread_("CameraModuleThread"),
      camera_module_callbacks_thread_("CameraModuleCallbacksThread") {
  VLOGF_ENTER();
  mojo::edk::Init();
  if (!ipc_thread_.StartWithOptions(
          base::Thread::Options(base::MessageLoop::TYPE_IO, 0))) {
    LOGF(ERROR) << "Mojo IPC thread failed to start";
    return;
  }
  mojo::edk::InitIPCSupport(this, ipc_thread_.task_runner());
  camera_module_callbacks_t::camera_device_status_change =
      CameraDeviceStatusChange;
}

CameraHalAdapter::~CameraHalAdapter() {
  VLOGF_ENTER();
  camera_module_thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&CameraHalAdapter::ResetModuleDelegateOnThread,
                            base::Unretained(this)));
  camera_module_callbacks_thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&CameraHalAdapter::ResetCallbacksDelegateOnThread,
                            base::Unretained(this)));
  camera_module_thread_.Stop();
  camera_module_callbacks_thread_.Stop();
  mojo::edk::ShutdownIPCSupport();
  ipc_thread_.Stop();
}

bool CameraHalAdapter::Start() {
  if (!socket_fd_.is_valid()) {
    LOGF(ERROR) << "Invalid socket fd: " << socket_fd_.get();
    return false;
  }
  mojo::edk::ScopedPlatformHandle handle(
      mojo::edk::PlatformHandle(socket_fd_.release()));

  // Make socket non-blocking.
  int flags = HANDLE_EINTR(fcntl(handle.get().handle, F_GETFL));
  if (flags == -1) {
    PLOG(ERROR) << "fcntl(F_GETFL)";
    return false;
  }
  if (HANDLE_EINTR(fcntl(handle.get().handle, F_SETFL, flags | O_NONBLOCK)) ==
      -1) {
    PLOG(ERROR) << "fcntl(F_SETFL) failed to enable O_NONBLOCK";
    return false;
  }

  // Set up mojo connection to child.
  mojo::edk::PlatformChannelPair channel_pair;
  mojo::edk::ChildProcessLaunched(0x0, channel_pair.PassServerHandle());

  mojo::edk::ScopedPlatformHandleVectorPtr handles(
      new mojo::edk::PlatformHandleVector(
          {channel_pair.PassClientHandle().release()}));

  std::string token = mojo::edk::GenerateRandomToken();
  LOGF(INFO) << "Generated token: " << token;
  mojo::ScopedMessagePipeHandle parent_pipe =
      mojo::edk::CreateParentMessagePipe(token);

  struct iovec iov = {const_cast<char*>(token.c_str()), token.length()};
  ssize_t result = mojo::edk::PlatformChannelSendmsgWithHandles(
      handle.get(), &iov, 1, handles->data(), handles->size());
  if (result == -1) {
    LOGF(ERROR) << "PlatformChannelSendmsgWithHandles failed: "
                << strerror(errno);
    return false;
  }

  if (!camera_module_thread_.Start()) {
    LOGF(ERROR) << "Failed to start CameraModuleThread";
    return false;
  }
  if (!camera_module_callbacks_thread_.Start()) {
    LOGF(ERROR) << "Failed to start CameraCallbacksThread";
    return false;
  }
  module_delegate_.reset(new CameraModuleDelegate(
      this, camera_module_thread_.task_runner(), quit_cb_));
  module_delegate_->Bind(std::move(parent_pipe));

  VLOGF(1) << "CameraHalAdapter started";
  return true;
}

void CameraHalAdapter::OnShutdownComplete() {
  ipc_thread_.Stop();
}

// Callback interface for camera_module_t APIs.

int32_t CameraHalAdapter::OpenDevice(int32_t device_id,
                                     mojom::Camera3DeviceOpsPtr* device_ops) {
  VLOGF_ENTER();
  if (device_id < 0) {
    LOGF(ERROR) << "Invalid camera device id: " << device_id;
    device_ops->reset();
    return -EINVAL;
  }
  if (device_adapters_.find(device_id) != device_adapters_.end()) {
    LOGF(WARNING) << "Multiple calls to OpenDevice on device " << device_id;
    device_ops->reset();
    return -EBUSY;
  }
  camera3_device_t* camera_device;
  char name[16];
  snprintf(name, sizeof(name), "%d", device_id);
  int32_t ret = camera_module_->common.methods->open(
      &camera_module_->common, name,
      reinterpret_cast<hw_device_t**>(&camera_device));
  if (ret) {
    LOGF(ERROR) << "Failed to open camera device " << device_id;
    device_ops->reset();
    return ret;
  } else {
    // This method is called by |camera_module_delegate_| on its mojo IPC
    // handler thread.
    // The CameraHalAdapter (and hence |camera_module_delegate_|) must out-live
    // the CameraDeviceAdapters, so it's safe to keep a reference to the task
    // runner of the current thread in the callback functor.
    base::Callback<void()> close_callback = base::Bind(
        &CameraHalAdapter::CloseDeviceCallback, base::Unretained(this),
        base::ThreadTaskRunnerHandle::Get(), device_id);
    device_adapters_[device_id].reset(
        new CameraDeviceAdapter(camera_device, close_callback));
    if (!device_adapters_[device_id]->Start()) {
      device_adapters_.erase(device_id);
      return -ENODEV;
    }
    *device_ops = device_adapters_.at(device_id)->GetDeviceOpsPtr();
  }
  return 0;
}

int32_t CameraHalAdapter::GetNumberOfCameras() {
  VLOGF_ENTER();
  return camera_module_->get_number_of_cameras();
}

int32_t CameraHalAdapter::GetCameraInfo(int32_t device_id,
                                        mojom::CameraInfoPtr* camera_info) {
  VLOGF_ENTER();

  if (device_id < 0) {
    LOGF(ERROR) << "Invalid camera device id: " << device_id;
    camera_info->reset();
    return -EINVAL;
  }
  camera_info_t info;
  int32_t ret = camera_module_->get_camera_info(device_id, &info);
  if (ret) {
    LOGF(ERROR) << "Failed to get info of camera " << device_id;
    camera_info->reset();
    return ret;
  }

  if (VLOG_IS_ON(1)) {
    dump_camera_metadata(info.static_camera_characteristics, 2, 3);
  }

  mojom::CameraInfoPtr info_ptr = mojom::CameraInfo::New();
  info_ptr->facing = static_cast<mojom::CameraFacing>(info.facing);
  info_ptr->orientation = info.orientation;
  info_ptr->device_version = info.device_version;
  info_ptr->static_camera_characteristics =
      internal::SerializeCameraMetadata(info.static_camera_characteristics);

  *camera_info = std::move(info_ptr);
  return 0;
}

int32_t CameraHalAdapter::SetCallbacks(
    mojom::CameraModuleCallbacksPtr callbacks) {
  VLOGF_ENTER();
  DCHECK(!callbacks_delegate_);
  callbacks_delegate_.reset(new CameraModuleCallbacksDelegate(
      callbacks.PassInterface(),
      camera_module_callbacks_thread_.task_runner()));
  return camera_module_->set_callbacks(this);
}

void CameraHalAdapter::CloseDeviceCallback(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    int32_t device_id) {
  task_runner->PostTask(FROM_HERE,
                        base::Bind(&CameraHalAdapter::CloseDevice,
                                   base::Unretained(this), device_id));
}

// static
void CameraHalAdapter::CameraDeviceStatusChange(
    const camera_module_callbacks_t* callbacks,
    int camera_id,
    int new_status) {
  VLOGF_ENTER();
  CameraHalAdapter* self = const_cast<CameraHalAdapter*>(
      static_cast<const CameraHalAdapter*>(callbacks));
  self->callbacks_delegate_->CameraDeviceStatusChange(camera_id, new_status);
}

void CameraHalAdapter::CloseDevice(int32_t device_id) {
  VLOGF_ENTER();
  if (device_id < 0) {
    LOGF(ERROR) << "Invalid camera device id: " << device_id;
    return;
  }
  if (device_adapters_.find(device_id) == device_adapters_.end()) {
    LOGF(ERROR) << "Failed to close camera device " << device_id
                << ": device is not opened";
    return;
  }
  device_adapters_.erase(device_id);
}

void CameraHalAdapter::ResetModuleDelegateOnThread() {
  DCHECK(camera_module_thread_.task_runner()->BelongsToCurrentThread());
  module_delegate_.reset();
}

void CameraHalAdapter::ResetCallbacksDelegateOnThread() {
  DCHECK(
      camera_module_callbacks_thread_.task_runner()->BelongsToCurrentThread());
  callbacks_delegate_.reset();
}

}  // namespace arc
