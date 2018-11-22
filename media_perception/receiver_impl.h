// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_PERCEPTION_RECEIVER_IMPL_H_
#define MEDIA_PERCEPTION_RECEIVER_IMPL_H_

#include <base/memory/shared_memory.h>
#include <map>
#include <memory>
#include <mojo/public/cpp/bindings/binding.h>
#include <string>

#include "base/logging.h"
#include "media_perception/shared_memory_provider.h"
#include "media_perception/video_capture_service_client.h"
#include "mojom/device_factory.mojom.h"

namespace mri {

class ReceiverImpl : public video_capture::mojom::Receiver {
 public:
  using FrameDataHandler = std::function<void(
      uint64_t timestamp_in_microseconds, const uint8_t* data, int data_size)>;
  ReceiverImpl() : binding_(this) {}

  // Creates a local proxy of the ReceiverPtr interface.
  video_capture::mojom::ReceiverPtr CreateInterfacePtr();

  // Sets the handler that will be called when new frames come from the device.
  void SetFrameHandler(FrameDataHandler frame_data_handler);

  // video_capture::mojom::Receiver overrides.
  void OnNewBuffer(int32_t buffer_id,
                   media::mojom::VideoBufferHandlePtr buffer_handle) override;
  void OnFrameReadyInBuffer(
      int32_t buffer_id, int32_t frame_feedback_id,
      video_capture::mojom::ScopedAccessPermissionPtr permission,
      media::mojom::VideoFrameInfoPtr frame_info) override;
  void OnFrameDropped(
      ::media::mojom::VideoCaptureFrameDropReason reason) override;
  void OnBufferRetired(int32_t buffer_id) override;
  void OnError(::media::mojom::VideoCaptureError error) override;
  void OnLog(const std::string& message) override;
  void OnStarted() override;
  void OnStartedUsingGpuDecode() override;

 private:
  // Frame handler for forwarding frames to the client.
  FrameDataHandler frame_data_handler_;

  // Binding of the Recevier interface to message pipe.
  mojo::Binding<video_capture::mojom::Receiver> binding_;

  std::map<int32_t /*buffer_id*/, std::unique_ptr<SharedMemoryProvider>>
      incoming_buffer_id_to_buffer_map_;
};

}  // namespace mri

#endif  // MEDIA_PERCEPTION_RECEIVER_IMPL_H_
