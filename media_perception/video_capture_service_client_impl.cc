// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media_perception/video_capture_service_client_impl.h"

#include <utility>
#include <base/single_thread_task_runner.h>

namespace mri {

void VideoCaptureServiceClientImpl::SetMojoConnector(
    MojoConnector* mojo_connector) {
  mojo_connector_ = mojo_connector;
}

bool VideoCaptureServiceClientImpl::Connect() {
  if (mojo_connector_ == nullptr) {
    LOG(ERROR) << "Mojo connector is nullptr.";
    return false;
  }
  mojo_connector_->ConnectToVideoCaptureService();
  return true;
}

bool VideoCaptureServiceClientImpl::IsConnected() {
  if (mojo_connector_ == nullptr)
    return false;

  return mojo_connector_->IsConnectedToVideoCaptureService();
}

void VideoCaptureServiceClientImpl::GetDevices(
    const GetDevicesCallback& callback) {
  mojo_connector_->GetDevices(callback);
}

void VideoCaptureServiceClientImpl::SetActiveDevice(
    const std::string& device_id, const SetActiveDeviceCallback& callback) {
  mojo_connector_->SetActiveDevice(device_id, callback);
}

void VideoCaptureServiceClientImpl::StartVideoCapture(
    const CaptureFormat& capture_format) {
  requested_frame_width_ = capture_format.frame_width;
  requested_frame_height_ = capture_format.frame_height;
  mojo_connector_->StartVideoCapture(
      capture_format, std::bind(&VideoCaptureServiceClientImpl::OnNewFrameData,
                                this, std::placeholders::_1,
                                std::placeholders::_2, std::placeholders::_3));
}

void VideoCaptureServiceClientImpl::StopVideoCapture() {
  mojo_connector_->StopVideoCapture();
}

void VideoCaptureServiceClientImpl::OnNewFrameData(
    uint64_t timestamp_in_microseconds, const uint8_t* data, int data_size) {
  if (frame_handler_ == nullptr) {
    LOG(ERROR) << "Frame handler is null but receiving frames.";
    return;
  }
  frame_handler_(timestamp_in_microseconds, data, data_size,
                 requested_frame_width_, requested_frame_height_);
}

void VideoCaptureServiceClientImpl::CreateVirtualDevice(
    const VideoDevice& video_device, const VirtualDeviceCallback& callback) {
  auto producer_impl = std::make_shared<ProducerImpl>();
  mojo_connector_->CreateVirtualDevice(video_device, producer_impl, callback);

  std::lock_guard<std::mutex> lock(device_id_to_producer_map_lock_);
  device_id_to_producer_map_.insert(
      std::make_pair(video_device.id, producer_impl));
}

void VideoCaptureServiceClientImpl::PushFrameToVirtualDevice(
    const std::string& device_id, uint64_t timestamp_in_microseconds,
    std::unique_ptr<const uint8_t[]> data, int data_size,
    PixelFormat pixel_format, int frame_width, int frame_height) {
  std::lock_guard<std::mutex> lock(device_id_to_producer_map_lock_);
  std::map<std::string, std::shared_ptr<ProducerImpl>>::iterator it =
      device_id_to_producer_map_.find(device_id);
  if (it == device_id_to_producer_map_.end()) {
    LOG(ERROR) << "Device id not found in producer map.";
    return;
  }
  mojo_connector_->PushFrameToVirtualDevice(
      it->second, base::TimeDelta::FromMicroseconds(timestamp_in_microseconds),
      std::move(data), data_size, pixel_format, frame_width, frame_height);
}

void VideoCaptureServiceClientImpl::CloseVirtualDevice(
    const std::string& device_id) {
  std::lock_guard<std::mutex> lock(device_id_to_producer_map_lock_);
  // Erasing the producer object will close the virtual device.
  device_id_to_producer_map_.erase(device_id);
}

}  // namespace mri
