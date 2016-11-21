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
#include <mojo/edk/embedder/embedder.h>
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
      socket_fd_(socket_fd),
      ipc_thread_("Mojo IPC thread") {
  VLOGF_ENTER();
  mojo::edk::Init();
  if (!ipc_thread_.StartWithOptions(
          base::Thread::Options(base::MessageLoop::TYPE_IO, 0))) {
    LOGF(ERROR) << "Mojo IPC thread failed to start";
    return;
  }
  mojo::edk::InitIPCSupport(this, ipc_thread_.task_runner());
  module_delegate_.reset(new CameraModuleDelegate(this, quit_cb));
}

CameraHalAdapter::~CameraHalAdapter() {
  VLOGF_ENTER();
  mojo::edk::ShutdownIPCSupport();
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

  char buf[1] = {};
  std::deque<mojo::edk::PlatformHandle> platform_handles;
  mojo::edk::PlatformChannelRecvmsg(handle.get(), buf, sizeof(buf),
                                    &platform_handles, true);
  if (platform_handles.size() != 1) {
    LOGF(ERROR) << "Unexpected number of handles received, expected 1: "
                << platform_handles.size();
    return false;
  }
  mojo::edk::ScopedPlatformHandle parent_pipe(platform_handles.back());
  platform_handles.pop_back();
  if (!parent_pipe.is_valid()) {
    LOGF(ERROR) << "Invalid parent pipe";
    return false;
  }
  mojo::edk::SetParentPipeHandle(std::move(parent_pipe));

  char token[64] = {0};
  mojo::edk::PlatformChannelRecvmsg(handle.get(), token, sizeof(token),
                                    &platform_handles, true);
  VLOGF(1) << "Got token: " << token;
  mojo::ScopedMessagePipeHandle child_pipe =
      mojo::edk::CreateChildMessagePipe(std::string(token));

  module_delegate_->Bind(std::move(child_pipe));

  VLOGF(1) << "CameraHalAdapter started";
  return true;
}

void CameraHalAdapter::OnShutdownComplete() {
  ipc_thread_.Stop();
}

// Callback interface for camera_module_t APIs.

mojom::OpenDeviceResultPtr CameraHalAdapter::OpenDevice(int32_t device_id) {
  VLOGF_ENTER();
  mojom::OpenDeviceResultPtr result = mojom::OpenDeviceResult::New();

  if (device_id < 0) {
    LOGF(ERROR) << "Invalid camera device id: " << device_id;
    result->set_error(-EINVAL);
    return result;
  }
  if (device_adapters_.find(device_id) != device_adapters_.end()) {
    LOGF(WARNING) << "Multiple calls to OpenDevice on device " << device_id;
    result->set_error(-EBUSY);
    return result;
  }
  camera3_device_t* camera_device;
  char name[16];
  snprintf(name, sizeof(name), "%d", device_id);
  int32_t ret = camera_module_->common.methods->open(
      &camera_module_->common, name,
      reinterpret_cast<hw_device_t**>(&camera_device));
  if (ret) {
    result->set_error(ret);
  } else {
    device_adapters_[device_id].reset(new CameraDeviceAdapter(camera_device));
    mojom::Camera3DeviceOpsPtr device_ops =
        device_adapters_.at(device_id)->GetDeviceOpsPtr();
    result->set_device_ops(std::move(device_ops));
  }
  return result;
}

int32_t CameraHalAdapter::CloseDevice(int32_t device_id) {
  VLOGF_ENTER();
  if (device_id < 0) {
    LOGF(ERROR) << "Invalid camera device id: " << device_id;
    return -EINVAL;
  }
  if (device_adapters_.find(device_id) == device_adapters_.end()) {
    LOGF(ERROR) << "Failed to close camera device " << device_id
                << ": device is not opened";
    return -ENODEV;
  }
  int32_t result = device_adapters_.at(device_id)->Close();
  device_adapters_.erase(device_id);
  return result;
}

int32_t CameraHalAdapter::GetNumberOfCameras() {
  VLOGF_ENTER();
  return camera_module_->get_number_of_cameras();
}

mojom::GetCameraInfoResultPtr CameraHalAdapter::GetCameraInfo(
    int32_t device_id) {
  VLOGF_ENTER();
  mojom::GetCameraInfoResultPtr result = mojom::GetCameraInfoResult::New();

  if (device_id < 0) {
    LOGF(ERROR) << "Invalid camera device id: " << device_id;
    result->set_error(-EINVAL);
    return result;
  }
  camera_info_t info;
  int32_t ret = camera_module_->get_camera_info(device_id, &info);
  if (ret) {
    result->set_error(ret);
    return result;
  }

  if (VLOG_IS_ON(1)) {
    dump_camera_metadata(info.static_camera_characteristics, 2, 3);
  }

  mojom::CameraInfoPtr info_ptr = mojom::CameraInfo::New();
  info_ptr->facing = info.facing;
  info_ptr->orientation = info.orientation;
  info_ptr->device_version = info.device_version;
  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  ret = internal::SerializeCameraMetadata(&producer_handle, &consumer_handle,
                                          info.static_camera_characteristics);
  if (ret) {
    LOGF(ERROR) << "Failed to send camera metadata in GetCameraInfo";
    // The returned error code is -EIO but HAL need -EINVAL.
    result->set_error(-EINVAL);
    return result;
  }
  info_ptr->metadata_handle = std::move(consumer_handle);

  result->set_info(std::move(info_ptr));
  return result;
}

int32_t CameraHalAdapter::SetCallbacks(
    mojom::CameraModuleCallbacksPtr callbacks) {
  VLOGF_ENTER();
  callbacks_delegate_.reset(
      new CameraModuleCallbacksDelegate(callbacks.PassInterface()));
  return camera_module_->set_callbacks(callbacks_delegate_.get());
}

}  // namespace arc
