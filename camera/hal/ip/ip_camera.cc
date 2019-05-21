/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <mojo/edk/embedder/embedder.h>
#include <utility>

#include "cros-camera/common.h"
#include "hal/ip/ip_camera.h"

namespace cros {

IpCamera::IpCamera()
    : listener_(nullptr), init_called_(false), binding_(this) {}

int IpCamera::Init(mojom::IpCameraDeviceRequest request) {
  ipc_task_runner_ = mojo::edk::GetIOTaskRunner();
  DCHECK(ipc_task_runner_->RunsTasksOnCurrentThread());

  if (!init_called_) {
    int ret = Init();
    if (ret) {
      return ret;
    }
    init_called_ = true;
  }

  binding_.Bind(std::move(request));
  binding_.set_connection_error_handler(
      base::Bind(&IpCamera::OnConnectionError, base::Unretained(this)));
  return 0;
}

IpCamera::~IpCamera() {
  DCHECK(ipc_task_runner_->RunsTasksOnCurrentThread());
  binding_.Close();
  listener_.reset();
}

void IpCamera::RegisterFrameListener(mojom::IpCameraFrameListenerPtr listener) {
  DCHECK(ipc_task_runner_->RunsTasksOnCurrentThread());
  listener_ = std::move(listener);
  listener_.set_connection_error_handler(
      base::Bind(&IpCamera::OnConnectionError, base::Unretained(this)));
}

void IpCamera::OnConnectionError() {
  LOGF(ERROR) << "Connection to IP camera was closed, stopping it";
  StopStreaming();
  binding_.Close();
  listener_.reset();
}

}  // namespace cros
