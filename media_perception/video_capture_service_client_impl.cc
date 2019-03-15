// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media_perception/video_capture_service_client_impl.h"

#include <utility>
#include <base/single_thread_task_runner.h>

#include "media_perception/device_management.pb.h"
#include "media_perception/proto_mojom_conversion.h"
#include "media_perception/serialized_proto.h"

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

void VideoCaptureServiceClientImpl::OpenDevice(
    const std::string& device_id,
    const SerializedVideoStreamParams& capture_format,
    const OpenDeviceCallback& callback) {
  VideoStreamParams format = Serialized<VideoStreamParams>(
      capture_format).Deserialize();

  std::lock_guard<std::mutex> lock(device_id_to_receiver_map_lock_);
  std::map<std::string, std::shared_ptr<ReceiverImpl>>::iterator it =
      device_id_to_receiver_map_.find(device_id);
  if (it != device_id_to_receiver_map_.end() &&
      it->second->HasValidCaptureFormat()) {
    LOG(INFO) << "Device with " << device_id << " already open.";
    SerializedVideoStreamParams current_format =
        Serialized<VideoStreamParams>(
            it->second->GetCaptureFormat()).GetBytes();

    // Device already open with the same settings.
    if (it->second->CaptureFormatsMatch(format)) {
      callback(
          device_id,
          CreatePushSubscriptionResultCode::CREATED_WITH_REQUESTED_SETTINGS,
          current_format);
      return;
    }
    // Device already open but with different settings.
    callback(
        device_id,
        CreatePushSubscriptionResultCode::CREATED_WITH_DIFFERENT_SETTINGS,
        current_format);
    return;
  }

  std::shared_ptr<ReceiverImpl> receiver_impl;
  if (it != device_id_to_receiver_map_.end()) {
    receiver_impl = it->second;
  } else {  // Create receiver if it doesn't exist.
    receiver_impl = std::make_shared<ReceiverImpl>();
  }
  device_id_to_receiver_map_.insert(
      std::make_pair(device_id, receiver_impl));
  mojo_connector_->OpenDevice(
      device_id, receiver_impl, format,
      std::bind(&VideoCaptureServiceClientImpl::OnOpenDeviceCallback,
                this, callback, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3));
}

void VideoCaptureServiceClientImpl::OnOpenDeviceCallback(
      const OpenDeviceCallback& callback,
      std::string device_id,
      CreatePushSubscriptionResultCode code,
      SerializedVideoStreamParams params) {
  VideoStreamParams format = Serialized<VideoStreamParams>(
      params).Deserialize();
  {
    std::lock_guard<std::mutex> lock(device_id_to_receiver_map_lock_);
    std::map<std::string, std::shared_ptr<ReceiverImpl>>::iterator it =
        device_id_to_receiver_map_.find(device_id);
    if (it != device_id_to_receiver_map_.end() &&
        code != CreatePushSubscriptionResultCode::FAILED) {
      it->second->SetCaptureFormat(format);
    }
  }
  callback(device_id, code, params);
}

bool VideoCaptureServiceClientImpl::IsVideoCaptureStartedForDevice(
    const std::string& device_id,
    SerializedVideoStreamParams* capture_format) {
  std::lock_guard<std::mutex> lock(device_id_to_receiver_map_lock_);
  std::map<std::string, std::shared_ptr<ReceiverImpl>>::iterator it =
      device_id_to_receiver_map_.find(device_id);
  bool capture_started = it != device_id_to_receiver_map_.end() &&
      it->second->HasValidCaptureFormat();
  if (capture_started) {
    *capture_format = Serialized<VideoStreamParams>(
        it->second->GetCaptureFormat()).GetBytes();
  }
  return capture_started;
}

int VideoCaptureServiceClientImpl::AddFrameHandler(
    const std::string& device_id,
    FrameHandler handler) {
  std::lock_guard<std::mutex> lock(device_id_to_receiver_map_lock_);

  std::map<std::string, std::shared_ptr<ReceiverImpl>>::iterator it =
      device_id_to_receiver_map_.find(device_id);
  if (it != device_id_to_receiver_map_.end()) {
    if (it->second->GetFrameHandlerCount() == 0) {
      // If no frame handlers exist for receiver, need to activate video
      // device.
      if (!mojo_connector_->ActivateDevice(device_id)) {
        // Failed to activate the device.
        return 0;
      }
    }
    return it->second->AddFrameHandler(std::move(handler));
  }
  return 0;
}

bool VideoCaptureServiceClientImpl::RemoveFrameHandler(
    const std::string& device_id, int frame_handler_id) {
  std::lock_guard<std::mutex> lock(device_id_to_receiver_map_lock_);
  std::map<std::string, std::shared_ptr<ReceiverImpl>>::iterator it =
      device_id_to_receiver_map_.find(device_id);

  if (it == device_id_to_receiver_map_.end()) {
    // Receiver does not exist. Ensure that the device is removed as well.
    mojo_connector_->StopVideoCapture(device_id);
    return false;
  }

  // Receiver does exist.
  bool success = it->second->RemoveFrameHandler(frame_handler_id);
  if (it->second->GetFrameHandlerCount() == 0) {
    // Remove the receiver object.
    device_id_to_receiver_map_.erase(device_id);
    // Stop video capture on the device.
    mojo_connector_->StopVideoCapture(device_id);
  }
  return success;
}

void VideoCaptureServiceClientImpl::CreateVirtualDevice(
    const SerializedVideoDevice& video_device,
    const VirtualDeviceCallback& callback) {
  std::lock_guard<std::mutex> lock(device_id_to_producer_map_lock_);
  VideoDevice device = Serialized<VideoDevice>(video_device).Deserialize();

  auto producer_impl = std::make_shared<ProducerImpl>();
  mojo_connector_->CreateVirtualDevice(device, producer_impl, callback);

  device_id_to_producer_map_.insert(
      std::make_pair(device.id(), producer_impl));
}

void VideoCaptureServiceClientImpl::PushFrameToVirtualDevice(
    const std::string& device_id, uint64_t timestamp_in_microseconds,
    std::unique_ptr<const uint8_t[]> data, int data_size,
    RawPixelFormat pixel_format, int frame_width, int frame_height) {
  std::lock_guard<std::mutex> lock(device_id_to_producer_map_lock_);
  std::map<std::string, std::shared_ptr<ProducerImpl>>::iterator it =
      device_id_to_producer_map_.find(device_id);
  if (it == device_id_to_producer_map_.end()) {
    LOG(ERROR) << "Device id not found in producer map.";
    return;
  }
  mojo_connector_->PushFrameToVirtualDevice(
      it->second, base::TimeDelta::FromMicroseconds(timestamp_in_microseconds),
      std::move(data), data_size, static_cast<PixelFormat>(pixel_format),
      frame_width, frame_height);
}

void VideoCaptureServiceClientImpl::CloseVirtualDevice(
    const std::string& device_id) {
  std::lock_guard<std::mutex> lock(device_id_to_producer_map_lock_);
  // Erasing the producer object will close the virtual device.
  device_id_to_producer_map_.erase(device_id);
}

}  // namespace mri
