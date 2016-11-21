/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hal_adapter/camera3_callback_ops_delegate.h"

#include <utility>

#include <base/bind.h>
#include <base/bind_helpers.h>

#include "arc/common.h"
#include "arc/future.h"
#include "hal_adapter/camera_device_adapter.h"

namespace arc {

Camera3CallbackOpsDelegate::Camera3CallbackOpsDelegate(
    CameraDeviceAdapter* camera_device_adapter,
    mojo::InterfacePtrInfo<mojom::Camera3CallbackOps> callback_ops_ptr_info)
    : internal::MojoInterfaceDelegate<mojom::Camera3CallbackOps>(
          std::move(callback_ops_ptr_info)),
      camera_device_adapter_(camera_device_adapter) {
  camera3_callback_ops_t::process_capture_result = ProcessCaptureResult;
  camera3_callback_ops_t::notify = Notify;
}

void Camera3CallbackOpsDelegate::ProcessCaptureResult(
    const camera3_callback_ops_t* ops,
    const camera3_capture_result_t* result) {
  VLOGF_ENTER();
  Camera3CallbackOpsDelegate* delegate =
      const_cast<Camera3CallbackOpsDelegate*>(
          static_cast<const Camera3CallbackOpsDelegate*>(ops));

  auto future =
      make_scoped_refptr(new internal::Future<void>(&delegate->relay_));
  delegate->thread_.task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&Camera3CallbackOpsDelegate::ProcessCaptureResultOnThread,
                 base::Unretained(delegate), base::Unretained(result),
                 internal::GetFutureCallback(future)));
  future->Wait();
}

void Camera3CallbackOpsDelegate::Notify(const camera3_callback_ops_t* ops,
                                        const camera3_notify_msg_t* msg) {
  VLOGF_ENTER();
  Camera3CallbackOpsDelegate* delegate =
      const_cast<Camera3CallbackOpsDelegate*>(
          static_cast<const Camera3CallbackOpsDelegate*>(ops));

  auto future =
      make_scoped_refptr(new internal::Future<void>(&delegate->relay_));
  delegate->thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&Camera3CallbackOpsDelegate::NotifyOnThread,
                            base::Unretained(delegate), base::Unretained(msg),
                            internal::GetFutureCallback(future)));
  future->Wait();
}

void Camera3CallbackOpsDelegate::ProcessCaptureResultOnThread(
    const camera3_capture_result_t* result,
    const base::Callback<void()>& cb) {
  interface_ptr_->ProcessCaptureResult(
      camera_device_adapter_->ProcessCaptureResult(result), cb);
}

void Camera3CallbackOpsDelegate::NotifyOnThread(
    const camera3_notify_msg_t* msg,
    const base::Callback<void()>& cb) {
  interface_ptr_->Notify(camera_device_adapter_->Notify(msg), cb);
}

}  // end of namespace arc
