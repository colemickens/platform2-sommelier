// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_PERCEPTION_MEDIA_PERCEPTION_IMPL_H_
#define MEDIA_PERCEPTION_MEDIA_PERCEPTION_IMPL_H_

#include <memory>
#include <string>

#include <mojo/public/cpp/bindings/binding.h>

#include "media_perception/rtanalytics.h"
#include "media_perception/video_capture_service_client.h"
#include "mojom/media_perception.mojom.h"

namespace mri {

class MediaPerceptionImpl :
  public chromeos::media_perception::mojom::MediaPerception {
 public:
  MediaPerceptionImpl(
      chromeos::media_perception::mojom::MediaPerceptionRequest request,
      std::shared_ptr<VideoCaptureServiceClient> vidcap_client,
      std::shared_ptr<Rtanalytics> rtanalytics);

  // chromeos::media_perception::mojom::MediaPerception:
  void SetupConfiguration(const std::string& configuration_name,
                          const SetupConfigurationCallback& callback) override;
  void GetVideoDevices(const GetVideoDevicesCallback& callback) override;
  void GetPipelineState(const std::string& configuration_name,
                        const GetPipelineStateCallback& callback) override;
  void SetPipelineState(
      const std::string& configuration_name,
      chromeos::media_perception::mojom::PipelineStatePtr desired_state,
      const SetPipelineStateCallback& callback) override;

  void set_connection_error_handler(base::Closure connection_error_handler);

 private:
  mojo::Binding<chromeos::media_perception::mojom::MediaPerception>
      binding_;

  std::shared_ptr<VideoCaptureServiceClient> vidcap_client_;

  std::shared_ptr<Rtanalytics> rtanalytics_;

  DISALLOW_COPY_AND_ASSIGN(MediaPerceptionImpl);
};

}  // namespace mri

#endif  // MEDIA_PERCEPTION_MEDIA_PERCEPTION_IMPL_H_
