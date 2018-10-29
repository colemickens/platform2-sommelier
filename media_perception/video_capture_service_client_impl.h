// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_PERCEPTION_VIDEO_CAPTURE_SERVICE_CLIENT_IMPL_H_
#define MEDIA_PERCEPTION_VIDEO_CAPTURE_SERVICE_CLIENT_IMPL_H_

#include "media_perception/video_capture_service_client.h"

#include <map>
#include <memory>
// NOLINTNEXTLINE
#include <mutex>
#include <string>

#include "media_perception/mojo_connector.h"
#include "media_perception/producer_impl.h"
#include "mojom/media_perception_service.mojom.h"

namespace mri {

// Implementation of the VideoCaptureServiceClient interface for interacting
// with the Chrome VideoCaptureService using google3 code.
class VideoCaptureServiceClientImpl : public VideoCaptureServiceClient {
 public:
  VideoCaptureServiceClientImpl() : mojo_connector_(nullptr) {}

  // Set the global mojo connector object for use with talking to the video
  // capture service.
  void SetMojoConnector(MojoConnector* mojo_connector);

  // VideoCaptureServiceClient overrides:
  bool Connect() override;
  bool IsConnected() override;
  void GetDevices(const GetDevicesCallback& callback) override;
  void SetActiveDevice(const std::string& device_id,
                       const SetActiveDeviceCallback& callback) override;
  void StartVideoCapture(
      const SerializedVideoStreamParams& capture_format) override;
  void StopVideoCapture() override;
  void CreateVirtualDevice(const SerializedVideoDevice& video_device,
                           const VirtualDeviceCallback& callback) override;
  void PushFrameToVirtualDevice(const std::string& device_id,
                                uint64_t timestamp_in_microseconds,
                                std::unique_ptr<const uint8_t[]> data,
                                int data_size,
                                RawPixelFormat pixel_format,
                                int frame_width, int frame_height) override;
  void CloseVirtualDevice(const std::string& device_id) override;

 private:
  void OnNewFrameData(uint64_t timestamp_in_microseconds, const uint8_t* data,
                      int data_size);

  MojoConnector* mojo_connector_;

  // Stores a map of device ids to producers for pushing frames to the correct
  // mojo object when PushFrameToVirtualDevice is called.
  // ProducerImpl objects provide an interface for buffer info updates of an
  // associated virtual device.
  std::map<std::string /*device_id*/, std::shared_ptr<ProducerImpl>>
      device_id_to_producer_map_;

  // Guards against concurrent changes to |device_id_to_producer_map_|.
  mutable std::mutex device_id_to_producer_map_lock_;

  // Stores the most recent requested frame width and height for incoming image
  // frames from the open active device.
  int requested_frame_width_;
  int requested_frame_height_;
};

}  // namespace mri

#endif  // MEDIA_PERCEPTION_VIDEO_CAPTURE_SERVICE_CLIENT_IMPL_H_
