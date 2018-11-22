// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media_perception/receiver_impl.h"

#include <utility>

#include "base/logging.h"

namespace mri {

void ReceiverImpl::SetFrameHandler(FrameDataHandler frame_data_handler) {
  frame_data_handler_ = std::move(frame_data_handler);
}

video_capture::mojom::ReceiverPtr ReceiverImpl::CreateInterfacePtr() {
  return binding_.CreateInterfacePtrAndBind();
}

void ReceiverImpl::OnNewBuffer(
    int32_t buffer_id, media::mojom::VideoBufferHandlePtr buffer_handle) {
  CHECK(buffer_handle->is_shared_memory_via_raw_file_descriptor());
  std::unique_ptr<SharedMemoryProvider> shared_memory_provider =
      SharedMemoryProvider::CreateFromRawFileDescriptor(
          true /*read_only*/,
          std::move(buffer_handle->get_shared_memory_via_raw_file_descriptor()
                        ->file_descriptor_handle),
          buffer_handle->get_shared_memory_via_raw_file_descriptor()
              ->shared_memory_size_in_bytes);
  if (!shared_memory_provider) {
    LOG(ERROR) << "SharedMemoryProvider is nullptr.";
    return;
  }
  incoming_buffer_id_to_buffer_map_.insert(
      std::make_pair(buffer_id, std::move(shared_memory_provider)));
}

void ReceiverImpl::OnFrameReadyInBuffer(
    int32_t buffer_id, int32_t frame_feedback_id,
    video_capture::mojom::ScopedAccessPermissionPtr permission,
    media::mojom::VideoFrameInfoPtr frame_info) {
  if (frame_data_handler_ == nullptr) {
    LOG(ERROR) << "Frame handler is null.";
    return;
  }

  SharedMemoryProvider* incoming_buffer =
      incoming_buffer_id_to_buffer_map_.at(buffer_id).get();
  frame_data_handler_(
      frame_info->timestamp->microseconds,
      static_cast<const uint8_t*>(
          incoming_buffer->GetSharedMemoryForInProcessAccess()->memory()),
      incoming_buffer->GetMemorySizeInBytes());
}

void ReceiverImpl::OnFrameDropped(
    ::media::mojom::VideoCaptureFrameDropReason reason) {
  LOG(WARNING) << "Got call to OnFrameDropped: " << reason;
}

void ReceiverImpl::OnBufferRetired(int32_t buffer_id) {
  incoming_buffer_id_to_buffer_map_.erase(buffer_id);
}

// The following methods are not needed to be implementated, as far as we know
// now.
void ReceiverImpl::OnError(::media::mojom::VideoCaptureError error) {
  LOG(ERROR) << "Got call to OnError: " << error;
}

void ReceiverImpl::OnLog(const std::string& message) {
  LOG(INFO) << "Got call to OnLog: " << message;
}

void ReceiverImpl::OnStarted() { LOG(INFO) << "Got call to OnStarted"; }

void ReceiverImpl::OnStartedUsingGpuDecode() {
  LOG(INFO) << "Got call on OnStartedUsingGpuDecode";
}

}  // namespace mri
