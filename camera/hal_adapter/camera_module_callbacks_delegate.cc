/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hal_adapter/camera_module_callbacks_delegate.h"

#include <utility>

#include <base/bind.h>
#include <base/bind_helpers.h>

#include "arc/common.h"

namespace arc {

CameraModuleCallbacksDelegate::CameraModuleCallbacksDelegate(
    mojo::InterfacePtrInfo<mojom::CameraModuleCallbacks> callbacks_ptr_info)
    : internal::MojoInterfaceDelegate<mojom::CameraModuleCallbacks>(
          std::move(callbacks_ptr_info)) {
  camera_module_callbacks_t::camera_device_status_change =
      CameraDeviceStatusChange;
}

void CameraModuleCallbacksDelegate::CameraDeviceStatusChange(
    const camera_module_callbacks_t* callbacks,
    int camera_id,
    int new_status) {
  VLOGF_ENTER();
  CameraModuleCallbacksDelegate* delegate =
      const_cast<CameraModuleCallbacksDelegate*>(
          static_cast<const CameraModuleCallbacksDelegate*>(callbacks));
  internal::Future<void> future;
  delegate->thread_.task_runner()->PostTask(
      FROM_HERE,
      base::Bind(
          &CameraModuleCallbacksDelegate::CameraDeviceStatusChangeOnThread,
          base::Unretained(delegate), camera_id, new_status,
          internal::GetFutureCallback(&future)));
  future.Wait();
}

void CameraModuleCallbacksDelegate::CameraDeviceStatusChangeOnThread(
    int camera_id,
    int new_status,
    const base::Callback<void()>& cb) {
  interface_ptr_->CameraDeviceStatusChange(camera_id, new_status, cb);
}

}  // namespace arc
