/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hal_adapter/camera_hal_adapter.h"

#include <algorithm>
#include <string>
#include <tuple>
#include <utility>

#include <base/bind.h>
#include <base/bind_helpers.h>
#include <base/logging.h>
#include <base/threading/thread_task_runner_handle.h>
#include <base/time/time.h>

#include "arc/common.h"
#include "arc/future.h"
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

constexpr base::TimeDelta kInitializationRetryDelay =
    base::TimeDelta::FromSeconds(3);

}  // namespace

CameraHalAdapter::CameraHalAdapter(std::vector<camera_module_t*> camera_modules)
    : camera_modules_(camera_modules),
      camera_module_thread_("CameraModuleThread"),
      camera_module_callbacks_thread_("CameraModuleCallbacksThread"),
      module_id_(0) {
  VLOGF_ENTER();
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

  auto future = internal::Future<bool>::Create(nullptr);
  camera_module_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&CameraHalAdapter::StartOnThread, base::Unretained(this),
                 internal::GetFutureCallback(future)));
  return future->Get();
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

  camera_module_t* camera_module;
  int internal_camera_id;
  std::tie(camera_module, internal_camera_id) =
      GetInternalModuleAndId(camera_id);

  if (!camera_module) {
    return -EINVAL;
  }

  if (device_adapters_.find(camera_id) != device_adapters_.end()) {
    LOGF(WARNING) << "Multiple calls to OpenDevice on device " << camera_id;
    return -EBUSY;
  }

  hw_module_t* common = &camera_module->common;
  camera3_device_t* camera_device;
  int ret =
      common->methods->open(common, std::to_string(internal_camera_id).c_str(),
                            reinterpret_cast<hw_device_t**>(&camera_device));
  if (ret != 0) {
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
  return camera_id_map_.size();
}

int32_t CameraHalAdapter::GetCameraInfo(int32_t camera_id,
                                        mojom::CameraInfoPtr* camera_info) {
  VLOGF_ENTER();

  camera_module_t* camera_module;
  int internal_camera_id;
  std::tie(camera_module, internal_camera_id) =
      GetInternalModuleAndId(camera_id);

  if (!camera_module) {
    camera_info->reset();
    return -EINVAL;
  }

  camera_info_t info;
  int ret = camera_module->get_camera_info(internal_camera_id, &info);
  if (ret != 0) {
    LOGF(ERROR) << "Failed to get info of camera " << camera_id;
    camera_info->reset();
    return ret;
  }

  LOGF(INFO) << "camera_id = " << camera_id << ", facing = " << info.facing;

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
    // To prevent some callbacks being fired too early, the call of
    // set_callbacks is delayed until the first client try to do so.  Note that
    // the other clients that comes later might still miss some callbacks, and
    // may cause some problems for external camera support in the future.
    for (size_t i = 0; i < camera_modules_.size(); i++) {
      auto aux = base::MakeUnique<CameraModuleCallbacksAux>();
      aux->camera_device_status_change = CameraDeviceStatusChange;
      aux->torch_mode_status_change = TorchModeStatusChange;
      aux->module_id = i;
      aux->adapter = this;
      int ret = camera_modules_[i]->set_callbacks(aux.get());
      if (ret != 0) {
        LOGF(ERROR) << "Failed to set camera module callbacks";
        return ret;
      }
      callbacks_auxs_.push_back(std::move(aux));
    }
  }
  callbacks_delegates_[callbacks_id] = std::move(callbacks_delegate);
  return 0;
}

int32_t CameraHalAdapter::SetTorchMode(int32_t camera_id, bool enabled) {
  VLOGF_ENTER();

  camera_module_t* camera_module;
  int internal_camera_id;
  std::tie(camera_module, internal_camera_id) =
      GetInternalModuleAndId(camera_id);

  if (!camera_module) {
    return -EINVAL;
  }

  if (auto fn = camera_module->set_torch_mode) {
    return fn(std::to_string(internal_camera_id).c_str(), enabled);
  }

  return -ENOSYS;
}

int32_t CameraHalAdapter::Init() {
  VLOGF_ENTER();
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
    int internal_camera_id,
    int new_status) {
  VLOGF_ENTER();

  auto* aux = static_cast<const CameraModuleCallbacksAux*>(callbacks);
  CameraHalAdapter* self = aux->adapter;
  int camera_id =
      self->camera_id_inverse_map_[aux->module_id][internal_camera_id];

  base::AutoLock l(self->callbacks_delegates_lock_);
  for (auto& it : self->callbacks_delegates_) {
    it.second->CameraDeviceStatusChange(camera_id, new_status);
  }
}

// static
void CameraHalAdapter::TorchModeStatusChange(
    const camera_module_callbacks_t* callbacks,
    const char* internal_camera_id,
    int new_status) {
  VLOGF_ENTER();

  auto* aux = static_cast<const CameraModuleCallbacksAux*>(callbacks);
  CameraHalAdapter* self = aux->adapter;
  int camera_id =
      self->camera_id_inverse_map_[aux->module_id][atoi(internal_camera_id)];

  base::AutoLock l(self->callbacks_delegates_lock_);
  for (auto& it : self->callbacks_delegates_) {
    it.second->TorchModeStatusChange(camera_id, new_status);
  }
}

void CameraHalAdapter::StartOnThread(base::Callback<void(bool)> callback) {
  VLOGF_ENTER();

  for (const auto& m : camera_modules_) {
    if (m->init) {
      int ret = m->init();
      if (ret != 0) {
        LOGF(ERROR) << "Failed to init camera module " << m->common.name;
        callback.Run(false);
        return;
      }
    }
  }

  std::vector<std::tuple<int, int, int>> cameras;

  camera_id_inverse_map_.resize(camera_modules_.size());
  for (size_t module_id = 0; module_id < camera_modules_.size(); module_id++) {
    camera_module_t* m = camera_modules_[module_id];

    int n = m->get_number_of_cameras();
    LOGF(INFO) << "Camera module " << module_id << " has " << n << " cameras";

    // TODO(shik): We may need to remove this check in the future to support
    // external cameras.  The initialization error shuold be detected in HAL,
    // not here.
    if (n == 0) {
      LOGF(WARNING) << "Number of cameras is zero. "
                    << "Assuming camera isn't initialized yet";
      sleep(kInitializationRetryDelay.InSeconds());
      callback.Run(false);
      return;
    }

    camera_id_inverse_map_[module_id].resize(n);

    for (int camera_id = 0; camera_id < n; camera_id++) {
      camera_info_t info;
      int ret = m->get_camera_info(camera_id, &info);
      if (ret != 0) {
        LOGF(ERROR) << "Failed to get info of camera " << camera_id
                    << " from module " << module_id;
        callback.Run(false);
        return;
      }
      cameras.emplace_back(info.facing, static_cast<int>(module_id), camera_id);
    }
  }

  sort(cameras.begin(), cameras.end());
  for (size_t i = 0; i < cameras.size(); i++) {
    int module_id = std::get<1>(cameras[i]);
    int camera_id = std::get<2>(cameras[i]);
    camera_id_map_.emplace_back(module_id, camera_id);
    camera_id_inverse_map_[module_id][camera_id] = i;
  }

  LOGF(INFO) << "SuperHAL started with " << camera_modules_.size()
             << " modules and " << camera_id_map_.size() << " cameras";

  callback.Run(true);
}

std::pair<camera_module_t*, int> CameraHalAdapter::GetInternalModuleAndId(
    int camera_id) {
  if (camera_id < 0 ||
      static_cast<size_t>(camera_id) >= camera_id_map_.size()) {
    LOGF(ERROR) << "Invalid camera id: " << camera_id;
    return {};
  }
  std::pair<int, int> idx = camera_id_map_[camera_id];
  return {camera_modules_[idx.first], idx.second};
}

void CameraHalAdapter::CloseDevice(int32_t camera_id) {
  VLOGF_ENTER();
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
