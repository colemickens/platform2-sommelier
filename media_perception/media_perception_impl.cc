// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media_perception/media_perception_impl.h"

#include <utility>
#include <vector>

#include "base/logging.h"
#include "media_perception/media_perception_mojom.pb.h"
#include "media_perception/proto_mojom_conversion.h"

namespace mri {

MediaPerceptionImpl::MediaPerceptionImpl(
    chromeos::media_perception::mojom::MediaPerceptionRequest request,
    std::shared_ptr<VideoCaptureServiceClient> vidcap_client,
    std::shared_ptr<Rtanalytics> rtanalytics)
    : binding_(this, std::move(request)),
      vidcap_client_(vidcap_client),
      rtanalytics_(rtanalytics) {
  if (!vidcap_client_->IsConnected()) {
    vidcap_client_->Connect();
  }
  CHECK(rtanalytics_.get()) << "Rtanalytics is a nullptr: "
                            << rtanalytics_.get();
}

void MediaPerceptionImpl::set_connection_error_handler(
    base::Closure connection_error_handler) {
  binding_.set_connection_error_handler(std::move(connection_error_handler));
}

void MediaPerceptionImpl::SetupConfiguration(
    const std::string& configuration_name,
    const SetupConfigurationCallback& callback) {
  SerializedSuccessStatus serialized_status;
  std::vector<PerceptionInterfaceType> interface_types =
      rtanalytics_->SetupConfiguration(
          configuration_name, &serialized_status);
  SuccessStatus status;
  status.ParseFromArray(serialized_status.data(), serialized_status.size());
  chromeos::media_perception::mojom::PerceptionInterfaceRequestsPtr
      requests_ptr =
      chromeos::media_perception::mojom::PerceptionInterfaceRequests::New();
  callback.Run(chromeos::media_perception::mojom::ToMojom(status),
               std::move(requests_ptr));
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

void MediaPerceptionImpl::GetPipelineState(
    const std::string& configuration_name,
    const GetPipelineStateCallback& callback) {
  SerializedPipelineState serialized_pipeline_state =
      rtanalytics_->GetPipelineState(configuration_name);

  PipelineState pipeline_state;
  pipeline_state.ParseFromArray(serialized_pipeline_state.data(),
                                serialized_pipeline_state.size());
  callback.Run(chromeos::media_perception::mojom::ToMojom(pipeline_state));
}

void MediaPerceptionImpl::SetPipelineState(
    const std::string& configuration_name,
    chromeos::media_perception::mojom::PipelineStatePtr desired_state,
    const SetPipelineStateCallback& callback) {
  SerializedPipelineState serialized_desired_state =
      SerializePipelineStateProto(ToProto(desired_state));
  SerializedPipelineState serialized_pipeline_state =
      rtanalytics_->SetPipelineState(
          configuration_name, &serialized_desired_state);

  PipelineState pipeline_state;
  pipeline_state.ParseFromArray(serialized_pipeline_state.data(),
                                serialized_pipeline_state.size());
  callback.Run(chromeos::media_perception::mojom::ToMojom(pipeline_state));
}

}  // namespace mri
