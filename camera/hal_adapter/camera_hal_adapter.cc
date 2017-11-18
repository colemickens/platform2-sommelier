/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hal_adapter/camera_hal_adapter.h"

#include <utility>

#include <base/bind.h>
#include <base/bind_helpers.h>
#include <base/logging.h>
#include <base/threading/thread_task_runner_handle.h>

#include "arc/common.h"
#include "hal_adapter/arc_camera3_mojo_utils.h"
#include "hal_adapter/camera_device_adapter.h"
#include "hal_adapter/camera_module_callbacks_delegate.h"
#include "hal_adapter/camera_module_delegate.h"

namespace arc {

namespace {

// A special id used in ResetModuleDelegateOnThread and
// ResetCallbacksDelegateOnThread to specify all the entries present in the
// |module_delegates_| and |callbacks_delegates_| maps.
const uint32_t kIdAll = 0xFFFFFFFF;

}  // namespace

CameraHalAdapter::CameraHalAdapter(camera_module_t* camera_module)
    : camera_module_(camera_module),
      camera_module_thread_("CameraModuleThread"),
      camera_module_callbacks_thread_("CameraModuleCallbacksThread"),
      module_id_(0) {
  VLOGF_ENTER();
  camera_module_callbacks_t::camera_device_status_change =
      CameraDeviceStatusChange;
}

CameraHalAdapter::~CameraHalAdapter() {
  VLOGF_ENTER();
  camera_module_thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&CameraHalAdapter::ResetModuleDelegateOnThread,
                            base::Unretained(this), kIdAll));
  camera_module_callbacks_thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&CameraHalAdapter::ResetCallbacksDelegateOnThread,
                            base::Unretained(this), kIdAll));
  camera_module_thread_.Stop();
  camera_module_callbacks_thread_.Stop();
}

bool CameraHalAdapter::Start() {
  VLOGF_ENTER();
  if (!camera_module_thread_.Start()) {
    LOGF(ERROR) << "Failed to start CameraModuleThread";
    return false;
  }
  if (!camera_module_callbacks_thread_.Start()) {
    LOGF(ERROR) << "Failed to start CameraCallbacksThread";
    return false;
  }
  VLOGF(1) << "CameraHalAdapter started";
  return true;
}

void CameraHalAdapter::OpenCameraHal(
    mojom::CameraModuleRequest camera_module_request) {
  VLOGF_ENTER();
  auto module_delegate = base::MakeUnique<CameraModuleDelegate>(
      this, camera_module_thread_.task_runner());
  uint32_t module_id = module_id_++;
  module_delegate->Bind(
      camera_module_request.PassMessagePipe(),
      base::Bind(&CameraHalAdapter::ResetModuleDelegateOnThread,
                 base::Unretained(this), module_id));
  base::AutoLock l(module_delegates_lock_);
  module_delegates_[module_id] = std::move(module_delegate);
  VLOGF(1) << "CameraModule " << module_id << " connected";
}

// Callback interface for camera_module_t APIs.

int32_t CameraHalAdapter::OpenDevice(
    int32_t camera_id, mojom::Camera3DeviceOpsRequest device_ops_request) {
  VLOGF_ENTER();
  if (camera_id < 0) {
    LOGF(ERROR) << "Invalid camera device id: " << camera_id;
    return -EINVAL;
  }
  if (device_adapters_.find(camera_id) != device_adapters_.end()) {
    LOGF(WARNING) << "Multiple calls to OpenDevice on device " << camera_id;
    return -EBUSY;
  }
  camera3_device_t* camera_device;
  char name[16];
  snprintf(name, sizeof(name), "%d", camera_id);
  int32_t ret = camera_module_->common.methods->open(
      &camera_module_->common, name,
      reinterpret_cast<hw_device_t**>(&camera_device));
  if (ret) {
    LOGF(ERROR) << "Failed to open camera device " << camera_id;
    return ret;
  }

  // This method is called by |camera_module_delegate_| on its mojo IPC
  // handler thread.
  // The CameraHalAdapter (and hence |camera_module_delegate_|) must out-live
  // the CameraDeviceAdapters, so it's safe to keep a reference to the task
  // runner of the current thread in the callback functor.
  base::Callback<void()> close_callback =
      base::Bind(&CameraHalAdapter::CloseDeviceCallback, base::Unretained(this),
                 base::ThreadTaskRunnerHandle::Get(), camera_id);
  device_adapters_[camera_id].reset(
      new CameraDeviceAdapter(camera_device, close_callback));
  if (!device_adapters_[camera_id]->Start()) {
    device_adapters_.erase(camera_id);
    return -ENODEV;
  }
  device_adapters_.at(camera_id)->Bind(std::move(device_ops_request));
  return 0;
}

int32_t CameraHalAdapter::GetNumberOfCameras() {
  VLOGF_ENTER();
  return camera_module_->get_number_of_cameras();
}

int32_t CameraHalAdapter::GetCameraInfo(int32_t camera_id,
                                        mojom::CameraInfoPtr* camera_info) {
  VLOGF_ENTER();

  if (camera_id < 0) {
    LOGF(ERROR) << "Invalid camera device id: " << camera_id;
    camera_info->reset();
    return -EINVAL;
  }
  camera_info_t info;
  int32_t ret = camera_module_->get_camera_info(camera_id, &info);
  if (ret) {
    LOGF(ERROR) << "Failed to get info of camera " << camera_id;
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
  auto callbacks_delegate = base::MakeUnique<CameraModuleCallbacksDelegate>(
      camera_module_callbacks_thread_.task_runner());
  uint32_t callbacks_id = callbacks_id_++;
  callbacks_delegate->Bind(
      callbacks.PassInterface(),
      base::Bind(&CameraHalAdapter::ResetCallbacksDelegateOnThread,
                 base::Unretained(this), callbacks_id));
  base::AutoLock l(callbacks_delegates_lock_);
  if (callbacks_delegates_.empty()) {
    int32_t ret = camera_module_->set_callbacks(this);
    if (ret) {
      LOGF(ERROR) << "Failed to set camera module callbacks";
      return ret;
    }
  }
  callbacks_delegates_[callbacks_id] = std::move(callbacks_delegate);
  return 0;
}

void CameraHalAdapter::CloseDeviceCallback(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    int32_t camera_id) {
  task_runner->PostTask(FROM_HERE,
                        base::Bind(&CameraHalAdapter::CloseDevice,
                                   base::Unretained(this), camera_id));
}

// static
void CameraHalAdapter::CameraDeviceStatusChange(
    const camera_module_callbacks_t* callbacks,
    int camera_id,
    int new_status) {
  VLOGF_ENTER();
  CameraHalAdapter* self = const_cast<CameraHalAdapter*>(
      static_cast<const CameraHalAdapter*>(callbacks));
  base::AutoLock l(self->callbacks_delegates_lock_);
  for (auto& it : self->callbacks_delegates_) {
    it.second->CameraDeviceStatusChange(camera_id, new_status);
  }
}

void CameraHalAdapter::CloseDevice(int32_t camera_id) {
  VLOGF_ENTER();
  if (camera_id < 0) {
    LOGF(ERROR) << "Invalid camera device id: " << camera_id;
    return;
  }
  if (device_adapters_.find(camera_id) == device_adapters_.end()) {
    LOGF(ERROR) << "Failed to close camera device " << camera_id
                << ": device is not opened";
    return;
  }
  device_adapters_.erase(camera_id);
}

void CameraHalAdapter::ResetModuleDelegateOnThread(uint32_t module_id) {
  VLOGF_ENTER();
  DCHECK(camera_module_thread_.task_runner()->BelongsToCurrentThread());
  base::AutoLock l(module_delegates_lock_);
  if (module_id == kIdAll) {
    module_delegates_.clear();
  } else {
    module_delegates_.erase(module_id);
  }
}

void CameraHalAdapter::ResetCallbacksDelegateOnThread(uint32_t callbacks_id) {
  VLOGF_ENTER();
  DCHECK(
      camera_module_callbacks_thread_.task_runner()->BelongsToCurrentThread());
  base::AutoLock l(callbacks_delegates_lock_);
  if (callbacks_id == kIdAll) {
    callbacks_delegates_.clear();
  } else {
    callbacks_delegates_.erase(callbacks_id);
  }
}

}  // namespace arc
