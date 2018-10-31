// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media_perception/media_perception_impl.h"

#include <utility>
#include <vector>

#include "base/logging.h"
#include "media_perception/proto_mojom_conversion.h"

namespace mri {

MediaPerceptionImpl::MediaPerceptionImpl(
    chromeos::media_perception::mojom::MediaPerceptionRequest request,
    std::shared_ptr<VideoCaptureServiceClient> vidcap_client)
    : binding_(this, std::move(request)),
      vidcap_client_(vidcap_client) {
  if (!vidcap_client_->IsConnected()) {
    vidcap_client_->Connect();
  }
}

void MediaPerceptionImpl::set_connection_error_handler(
    base::Closure connection_error_handler) {
  binding_.set_connection_error_handler(std::move(connection_error_handler));
}

void MediaPerceptionImpl::GetVideoDevices(
    const GetVideoDevicesCallback& callback) {
  // Get the list of video devices from the Video Capture Service client and
  // convert them to mojom object.
  vidcap_client_->GetDevices([=](std::vector<SerializedVideoDevice> devices) {
    std::vector<chromeos::media_perception::mojom::VideoDevicePtr>
        mojom_devices;
    for (const SerializedVideoDevice& device : devices) {
      VideoDevice video_device;
      video_device.ParseFromArray(device.data(), device.size());
      mojom_devices.push_back(
          chromeos::media_perception::mojom::ToMojom(video_device));
    }
    callback.Run(std::move(mojom_devices));
  });
}

}  // namespace mri
