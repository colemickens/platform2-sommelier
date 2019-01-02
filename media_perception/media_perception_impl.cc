// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media_perception/media_perception_impl.h"

#include <utility>
#include <vector>

#include "base/logging.h"
#include "media_perception/media_perception_mojom.pb.h"
#include "media_perception/proto_mojom_conversion.h"
#include "media_perception/serialized_proto.h"

namespace mri {

MediaPerceptionImpl::MediaPerceptionImpl(
    chromeos::media_perception::mojom::MediaPerceptionRequest request,
    std::shared_ptr<VideoCaptureServiceClient> vidcap_client,
    std::shared_ptr<ChromeAudioServiceClient> cras_client,
    std::shared_ptr<Rtanalytics> rtanalytics)
    : binding_(this, std::move(request)),
      vidcap_client_(vidcap_client),
      cras_client_(cras_client),
      rtanalytics_(rtanalytics) {
  if (!vidcap_client_->IsConnected()) {
    vidcap_client_->Connect();
  }

  if (!cras_client_->IsConnected()) {
    cras_client_->Connect();
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
  SuccessStatus status = Serialized<SuccessStatus>(
      serialized_status).Deserialize();
  chromeos::media_perception::mojom::PerceptionInterfaceRequestsPtr
      requests_ptr =
      chromeos::media_perception::mojom::PerceptionInterfaceRequests::New();
  callback.Run(chromeos::media_perception::mojom::ToMojom(status),
               std::move(requests_ptr));
}

void MediaPerceptionImpl::GetVideoDevices(
    const GetVideoDevicesCallback& callback) {
  // Get the list of video devices from the VideoCaptureServiceClient and
  // convert them to mojom objects.
  vidcap_client_->GetDevices([=](std::vector<SerializedVideoDevice> devices) {
    std::vector<chromeos::media_perception::mojom::VideoDevicePtr>
        mojom_devices;
    for (const SerializedVideoDevice& device : devices) {
      VideoDevice video_device = Serialized<VideoDevice>(device).Deserialize();
      mojom_devices.push_back(
          chromeos::media_perception::mojom::ToMojom(video_device));
    }
    callback.Run(std::move(mojom_devices));
  });
}

void MediaPerceptionImpl::GetAudioDevices(
    const GetAudioDevicesCallback& callback) {
  // Get the list of audio devices from the ChromeAudioServiceClient and convert
  // them to mojom objects.
  std::vector<SerializedAudioDevice> devices = cras_client_->GetInputDevices();
  std::vector<chromeos::media_perception::mojom::AudioDevicePtr> mojom_devices;
  for (const SerializedAudioDevice& device : devices) {
    AudioDevice audio_device = Serialized<AudioDevice>(device).Deserialize();
    mojom_devices.push_back(
        chromeos::media_perception::mojom::ToMojom(audio_device));
  }
  callback.Run(std::move(mojom_devices));
}

void MediaPerceptionImpl::GetTemplateDevices(
    const std::string& configuration_name,
    const GetTemplateDevicesCallback& callback) {
  std::vector<SerializedDeviceTemplate> device_templates =
      rtanalytics_->GetTemplateDevices(configuration_name);
  std::vector<chromeos::media_perception::mojom::DeviceTemplatePtr>
      template_ptrs;
  for (const auto& serialized_device_template : device_templates) {
    DeviceTemplate device_template = Serialized<DeviceTemplate>(
        serialized_device_template).Deserialize();
    template_ptrs.push_back(
        chromeos::media_perception::mojom::ToMojom(device_template));
  }
  callback.Run(std::move(template_ptrs));
}

void MediaPerceptionImpl::SetVideoDeviceForTemplateName(
    const std::string& configuration_name,
    const std::string& template_name,
    chromeos::media_perception::mojom::VideoDevicePtr device,
    const SetVideoDeviceForTemplateNameCallback& callback) {
  SerializedVideoDevice serialized_video_device =
      Serialized<VideoDevice>(ToProto(device)).GetBytes();
  SerializedSuccessStatus status = rtanalytics_->SetVideoDeviceForTemplateName(
      configuration_name, template_name, serialized_video_device);

  SuccessStatus success_status = Serialized<SuccessStatus>(
      status).Deserialize();
  callback.Run(chromeos::media_perception::mojom::ToMojom(success_status));
}

void MediaPerceptionImpl::SetAudioDeviceForTemplateName(
    const std::string& configuration_name,
    const std::string& template_name,
    chromeos::media_perception::mojom::AudioDevicePtr device,
    const SetAudioDeviceForTemplateNameCallback& callback) {
  SerializedAudioDevice serialized_audio_device =
      Serialized<AudioDevice>(ToProto(device)).GetBytes();
  SerializedSuccessStatus status = rtanalytics_->SetAudioDeviceForTemplateName(
      configuration_name, template_name, serialized_audio_device);

  SuccessStatus success_status = Serialized<SuccessStatus>(
      status).Deserialize();
  callback.Run(chromeos::media_perception::mojom::ToMojom(success_status));
}

void MediaPerceptionImpl::SetVirtualVideoDeviceForTemplateName(
    const std::string& configuration_name,
    const std::string& template_name,
    chromeos::media_perception::mojom::VirtualVideoDevicePtr device,
    const SetVirtualVideoDeviceForTemplateNameCallback& callback) {
  SerializedVirtualVideoDevice serialized_virtual_video_device =
      Serialized<VirtualVideoDevice>(ToProto(device)).GetBytes();
  SerializedSuccessStatus status =
      rtanalytics_->SetVirtualVideoDeviceForTemplateName(
      configuration_name, template_name, serialized_virtual_video_device);

  SuccessStatus success_status = Serialized<SuccessStatus>(
      status).Deserialize();
  callback.Run(chromeos::media_perception::mojom::ToMojom(success_status));
}

void MediaPerceptionImpl::GetPipelineState(
    const std::string& configuration_name,
    const GetPipelineStateCallback& callback) {
  SerializedPipelineState serialized_pipeline_state =
      rtanalytics_->GetPipelineState(configuration_name);

  PipelineState pipeline_state = Serialized<PipelineState>(
      serialized_pipeline_state).Deserialize();
  callback.Run(chromeos::media_perception::mojom::ToMojom(pipeline_state));
}

void MediaPerceptionImpl::SetPipelineState(
    const std::string& configuration_name,
    chromeos::media_perception::mojom::PipelineStatePtr desired_state,
    const SetPipelineStateCallback& callback) {
  SerializedPipelineState serialized_desired_state =
      Serialized<PipelineState>(ToProto(desired_state)).GetBytes();
  SerializedPipelineState serialized_pipeline_state =
      rtanalytics_->SetPipelineState(
          configuration_name, serialized_desired_state);

  PipelineState pipeline_state = Serialized<PipelineState>(
      serialized_pipeline_state).Deserialize();
  callback.Run(chromeos::media_perception::mojom::ToMojom(pipeline_state));
}

}  // namespace mri
