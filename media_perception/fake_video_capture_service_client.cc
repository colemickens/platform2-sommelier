// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media_perception/fake_video_capture_service_client.h"

namespace mri {

bool FakeVideoCaptureServiceClient::Connect() {
  connected_ = true;
  return connected_;
}

bool FakeVideoCaptureServiceClient::IsConnected() {
  return connected_;
}

void FakeVideoCaptureServiceClient::SetDevicesForGetDevices(
    std::vector<SerializedVideoDevice> devices) {
  devices_ = devices;
}

void FakeVideoCaptureServiceClient::GetDevices(
    const GetDevicesCallback& callback) {
  callback(devices_);
}

void FakeVideoCaptureServiceClient::SetActiveDevice(
    const std::string& device_id,
    const SetActiveDeviceCallback& callback) {}

void FakeVideoCaptureServiceClient::StartVideoCapture(
    const SerializedVideoStreamParams& capture_format) {}

void FakeVideoCaptureServiceClient::StopVideoCapture() {}

void FakeVideoCaptureServiceClient::CreateVirtualDevice(
    const SerializedVideoDevice& video_device,
    const VirtualDeviceCallback& callback) {}

void FakeVideoCaptureServiceClient::PushFrameToVirtualDevice(
    const std::string& device_id,
    uint64_t timestamp_in_microseconds,
    std::unique_ptr<const uint8_t[]> data,
    int data_size,
    RawPixelFormat pixel_format,
    int frame_width, int frame_height) {}

void FakeVideoCaptureServiceClient::CloseVirtualDevice(
    const std::string& device_id) {}

}  // namespace mri
