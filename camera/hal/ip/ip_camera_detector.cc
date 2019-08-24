/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <utility>
#include <mojo/edk/embedder/embedder.h>

#include "cros-camera/common.h"
#include "hal/ip/ip_camera_detector.h"
#include "hal/ip/mock_frame_generator.h"

namespace cros {

IpCameraDetectorImpl::IpCameraDetectorImpl()
    : binding_(this), listener_(nullptr), next_camera_id_(0) {}

int IpCameraDetectorImpl::Init(mojom::IpCameraDetectorRequest request) {
  ipc_task_runner_ = mojo::edk::GetIOTaskRunner();
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  binding_.Bind(std::move(request));

  // TODO(pceballos): For now just add the mock device on startup after a short
  // sleep.
  ipc_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&IpCameraDetectorImpl::MockFrameGeneratorConnect,
                     base::Unretained(this)),
      base::TimeDelta::FromSeconds(5));
  return 0;
}

IpCameraDetectorImpl::~IpCameraDetectorImpl() {
  // This destructor will dead-lock if called on the IPC thread, or if the IPC
  // thread is no longer running when it's called.
  DCHECK(!ipc_task_runner_->BelongsToCurrentThread());
  auto return_val = Future<void>::Create(nullptr);
  ipc_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&IpCameraDetectorImpl::DestroyOnIpcThread,
                                base::Unretained(this), return_val));
  return_val->Wait(-1);
}

void IpCameraDetectorImpl::DestroyOnIpcThread(
    scoped_refptr<Future<void>> return_val) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  listener_.reset();
  binding_.Close();
  return_val->Set();
}

void IpCameraDetectorImpl::RegisterConnectionListener(
    mojom::IpCameraConnectionListenerPtr listener) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  listener_ = std::move(listener);

  // TODO(pceballos): detect and handle a listener disconnecting and a new one
  // connecting. I think the bindings/pointers need to be destroyed and
  // re-generated.

  // When a listener is first registered, send OnDeviceConnected callbacks for
  // all of the already connected devices.
  for (auto& device_entry : devices_) {
    int32_t id = device_entry.first;
    std::unique_ptr<IpCamera>& device = device_entry.second;

    mojom::IpCameraDevicePtr device_ptr;
    int ret = device->Init(mojo::MakeRequest(&device_ptr));
    if (ret != 0) {
      continue;
    }

    listener_->OnDeviceConnected(
        id, std::move(device_ptr),
        mojom::IpCameraStream::New(device->GetFormat(), device->GetWidth(),
                                   device->GetHeight(), device->GetFps()));
  }
}

void IpCameraDetectorImpl::MockFrameGeneratorConnect() {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  std::unique_ptr<IpCamera> device = std::make_unique<MockFrameGenerator>();

  if (listener_) {
    mojom::IpCameraDevicePtr device_ptr;
    int ret = device->Init(mojo::MakeRequest(&device_ptr));
    if (ret != 0) {
      return;
    }

    listener_->OnDeviceConnected(
        next_camera_id_, std::move(device_ptr),
        mojom::IpCameraStream::New(device->GetFormat(), device->GetWidth(),
                                   device->GetHeight(), device->GetFps()));
  }
  devices_[next_camera_id_] = std::move(device);

  next_camera_id_++;
}

void IpCameraDetectorImpl::DeviceDisconnect(int32_t id) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  if (devices_.find(id) == devices_.end()) {
    LOGF(ERROR) << "Invalid camera id " << id;
    return;
  }

  if (listener_) {
    listener_->OnDeviceDisconnected(id);
  }

  devices_.erase(id);
}

}  // namespace cros
