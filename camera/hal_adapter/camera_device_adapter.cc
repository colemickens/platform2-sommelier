/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hal_adapter/camera_device_adapter.h"

#include <map>
#include <set>
#include <utility>
#include <vector>

#include <base/bind.h>
#include <base/bind_helpers.h>
#include <base/logging.h>

#include "arc/common.h"
#include "hal_adapter/camera3_callback_ops_delegate.h"
#include "hal_adapter/camera3_device_ops_delegate.h"

namespace arc {

CameraDeviceAdapter::CameraDeviceAdapter(camera3_device_t* camera_device)
    : camera_device_(camera_device) {
  VLOGF_ENTER() << ":" << camera_device_;
  device_ops_delegate_.reset(new Camera3DeviceOpsDelegate(this));
}

CameraDeviceAdapter::~CameraDeviceAdapter() {
  VLOGF_ENTER() << ":" << camera_device_;
}

mojom::Camera3DeviceOpsPtr CameraDeviceAdapter::GetDeviceOpsPtr() {
  return device_ops_delegate_->CreateInterfacePtr();
}

int CameraDeviceAdapter::Close() {
  // Close the device.
  return camera_device_->common.close(&camera_device_->common);
}

int32_t CameraDeviceAdapter::Initialize(
    mojom::Camera3CallbackOpsPtr callback_ops) {
  VLOGF_ENTER();
  callback_ops_delegate_.reset(
      new Camera3CallbackOpsDelegate(this, callback_ops.PassInterface()));
  return camera_device_->ops->initialize(camera_device_,
                                         callback_ops_delegate_.get());
}

mojom::Camera3StreamConfigurationPtr CameraDeviceAdapter::ConfigureStreams(
    mojom::Camera3StreamConfigurationPtr config) {
  VLOGF_ENTER();

  base::AutoLock l(streams_lock_);

  std::map<uint64_t, std::unique_ptr<camera3_stream_t>> new_streams;
  for (const auto& s : config->streams) {
    uint64_t id = s->id;
    std::unique_ptr<camera3_stream_t>& stream = new_streams[id];
    auto it = streams_.find(id);
    if (it == streams_.end()) {
      VLOGF(1) << "Add new stream: " << id;
      stream.reset(new camera3_stream_t);
    } else {
      VLOGF(1) << "Update existing stream: " << id;
      stream.swap(it->second);
    }
    stream->stream_type = s->stream_type;
    stream->width = s->width;
    stream->height = s->height;
    stream->format = s->format;
    stream->usage = s->usage;
    stream->max_buffers = s->max_buffers;
    stream->data_space = static_cast<android_dataspace_t>(s->data_space);
    stream->rotation = s->rotation;
  }
  streams_.swap(new_streams);

  camera3_stream_configuration_t stream_list;
  stream_list.num_streams = config->num_streams;
  stream_list.streams = new camera3_stream_t*[config->num_streams];
  stream_list.operation_mode = config->operation_mode;
  int i = 0;
  for (auto it = streams_.begin(); it != streams_.end(); it++) {
    stream_list.streams[i++] = it->second.get();
  }

  mojom::Camera3StreamConfigurationPtr updated_config =
      mojom::Camera3StreamConfiguration::New();

  updated_config->error =
      camera_device_->ops->configure_streams(camera_device_, &stream_list);

  delete[](stream_list.streams);

  if (!updated_config->error) {
    updated_config->num_streams = streams_.size();
    for (const auto& s : streams_) {
      mojom::Camera3StreamPtr ptr = mojom::Camera3Stream::New();
      ptr->id = s.first;
      // HAL should only change usage and max_buffers.
      ptr->usage = s.second->usage;
      ptr->max_buffers = s.second->max_buffers;
      updated_config->streams.push_back(std::move(ptr));
    }
  }

  return updated_config;
}

mojom::CameraMetadataPtr CameraDeviceAdapter::ConstructDefaultRequestSettings(
    int32_t type) {
  VLOGF_ENTER();
  const camera_metadata_t* metadata =
      camera_device_->ops->construct_default_request_settings(camera_device_,
                                                              type);
  mojom::CameraMetadataPtr settings = mojom::CameraMetadata::New();
  if (!metadata) {
    settings->set_none(true);
  } else {
    mojo::ScopedDataPipeProducerHandle producer_handle;
    mojo::ScopedDataPipeConsumerHandle consumer_handle;
    int ret = internal::SerializeCameraMetadata(&producer_handle,
                                                &consumer_handle, metadata);
    if (ret) {
      settings->set_none(true);
    } else {
      settings->set_metadata_handle(std::move(consumer_handle));
    }
  }
  return settings;
}

int32_t CameraDeviceAdapter::ProcessCaptureRequest(
    mojom::Camera3CaptureRequestPtr request) {
  VLOGF_ENTER();
  camera3_capture_request_t req;

  req.frame_number = request->frame_number;

  internal::CameraMetadataUniquePtr settings;
  if (request->settings->is_none()) {
    req.settings = nullptr;
  } else {
    settings = internal::DeserializeCameraMetadata(
        request->settings->get_metadata_handle().get());
    if (!settings) {
      return -EINVAL;
    }
    req.settings = settings.get();
  }

  // Deserialize input buffer.
  std::unique_ptr<camera3_stream_buffer_t> input_buffer;
  if (request->input_buffer->is_none()) {
    req.input_buffer = nullptr;
  } else {
    base::AutoLock streams_lock(streams_lock_);
    base::AutoLock buffer_handles_lock(buffer_handles_lock_);
    mojom::Camera3StreamBufferPtr& in_buf_ptr =
        request->input_buffer->get_input_buffer();
    input_buffer.reset(new camera3_stream_buffer_t);
    buffer_handle_t input_buffer_handle = native_handle_create(
        in_buf_ptr->buffer->num_fds, in_buf_ptr->buffer->num_ints);
    buffer_handles_[input_buffer_handle] = in_buf_ptr->buffer->handle_id;
    input_buffer->buffer = &input_buffer_handle;
    internal::DeserializeStreamBuffer(in_buf_ptr, streams_, input_buffer.get());
    req.input_buffer = input_buffer.get();
  }

  // Deserialize output buffers.
  req.num_output_buffers = request->num_output_buffers;

  std::vector<camera3_stream_buffer_t> output_buffers(
      request->num_output_buffers);
  std::vector<buffer_handle_t> output_buffer_handles(
      request->num_output_buffers);
  {
    base::AutoLock streams_lock(streams_lock_);
    base::AutoLock buffer_handles_lock(buffer_handles_lock_);
    for (unsigned int i = 0; i < request->num_output_buffers; ++i) {
      mojom::Camera3StreamBufferPtr& out_buf_ptr =
          request->output_buffers->get_output_buffers()[i];
      buffer_handle_t output_buffer_handle = native_handle_create(
          out_buf_ptr->buffer->num_fds, out_buf_ptr->buffer->num_ints);
      buffer_handles_[output_buffer_handle] = out_buf_ptr->buffer->handle_id;
      output_buffer_handles.at(i) = output_buffer_handle;
      output_buffers.at(i).buffer =
          const_cast<const native_handle_t**>(&output_buffer_handles.at(i));
      internal::DeserializeStreamBuffer(out_buf_ptr, streams_,
                                        &output_buffers.at(i));
    }
    req.output_buffers =
        const_cast<const camera3_stream_buffer_t*>(output_buffers.data());
  }

  return camera_device_->ops->process_capture_request(camera_device_, &req);
}

void CameraDeviceAdapter::Dump(mojo::ScopedHandle fd) {
  VLOGF_ENTER();
  base::ScopedFD dump_fd(internal::UnwrapPlatformHandle(std::move(fd)));
  camera_device_->ops->dump(camera_device_, dump_fd.get());
}

int32_t CameraDeviceAdapter::Flush() {
  VLOGF_ENTER();
  return camera_device_->ops->flush(camera_device_);
}

mojom::Camera3CaptureResultPtr CameraDeviceAdapter::ProcessCaptureResult(
    const camera3_capture_result_t* result) {
  mojom::Camera3CaptureResultPtr r = mojom::Camera3CaptureResult::New();

  r->frame_number = result->frame_number;

  mojom::CameraMetadataPtr metadata = mojom::CameraMetadata::New();
  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  if (!result->result) {
    metadata->set_none(true);
  } else {
    int ret = internal::SerializeCameraMetadata(
        &producer_handle, &consumer_handle, result->result);
    if (ret) {
      metadata->set_none(true);
    } else {
      metadata->set_metadata_handle(std::move(consumer_handle));
    }
  }
  r->result = std::move(metadata);

  // num_output_buffers may be 0.
  r->num_output_buffers = result->num_output_buffers;

  // Serialize output buffers.  This may be none as num_output_buffers may be 0.
  mojom::Camera3CaptureOutputBuffersPtr output_buffers =
      mojom::Camera3CaptureOutputBuffers::New();
  if (!result->output_buffers) {
    output_buffers->set_none(true);
  } else {
    base::AutoLock streams_lock(streams_lock_);
    base::AutoLock buffer_handles_lock(buffer_handles_lock_);
    mojo::Array<mojom::Camera3StreamBufferPtr> output_buffers_array;
    for (unsigned int i = 0; i < result->num_output_buffers; i++) {
      mojom::Camera3StreamBufferPtr out_buf = internal::SerializeStreamBuffer(
          result->output_buffers + i, streams_, buffer_handles_);
      if (out_buf.is_null()) {
        LOGF(ERROR) << "Failed to serialize output stream buffer";
        // TODO(jcliang): Handle error?
      }
      buffer_handle_t handle = *(result->output_buffers + i)->buffer;
      buffer_handles_.erase(handle);
      native_handle_close(const_cast<native_handle_t*>(handle));
      native_handle_delete(const_cast<native_handle_t*>(handle));
      output_buffers_array.push_back(std::move(out_buf));
    }
    output_buffers->set_output_buffers(std::move(output_buffers_array));
  }
  r->output_buffers = std::move(output_buffers);

  // Serialize input buffer.
  mojom::Camera3CaptureInputBufferPtr input_buffer =
      mojom::Camera3CaptureInputBuffer::New();
  if (!result->input_buffer) {
    input_buffer->set_none(true);
  } else {
    base::AutoLock streams_lock(streams_lock_);
    base::AutoLock buffer_handles_lock(buffer_handles_lock_);
    mojom::Camera3StreamBufferPtr in_buf = internal::SerializeStreamBuffer(
        result->input_buffer, streams_, buffer_handles_);
    if (in_buf.is_null()) {
      LOGF(ERROR) << "Failed to serialize input stream buffer";
    }
    input_buffer->set_input_buffer(std::move(in_buf));
    buffer_handle_t handle = *result->input_buffer->buffer;
    buffer_handles_.erase(handle);
    native_handle_close(const_cast<native_handle_t*>(handle));
    native_handle_delete(const_cast<native_handle_t*>(handle));
  }
  r->input_buffer = std::move(input_buffer);

  r->partial_result = result->partial_result;

  return r;
}

mojom::Camera3NotifyMsgPtr CameraDeviceAdapter::Notify(
    const camera3_notify_msg_t* msg) {
  // Fill in the data from msg...
  mojom::Camera3NotifyMsgPtr m = mojom::Camera3NotifyMsg::New();
  m->type = msg->type;
  m->message = mojom::Camera3NotifyMsgMessage::New();

  if (msg->type == CAMERA3_MSG_ERROR) {
    mojom::Camera3ErrorMsgPtr error = mojom::Camera3ErrorMsg::New();
    error->frame_number = msg->message.error.frame_number;
    uint64_t stream_id = 0;
    {
      base::AutoLock l(streams_lock_);
      for (const auto& s : streams_) {
        if (s.second.get() == msg->message.error.error_stream) {
          stream_id = s.first;
          break;
        }
      }
    }
    error->error_stream_id = stream_id;
    error->error_code = msg->message.error.error_code;
    m->message->set_error(std::move(error));
  } else if (msg->type == CAMERA3_MSG_SHUTTER) {
    mojom::Camera3ShutterMsgPtr shutter = mojom::Camera3ShutterMsg::New();
    shutter->frame_number = msg->message.shutter.frame_number;
    shutter->timestamp = msg->message.shutter.timestamp;
    m->message->set_shutter(std::move(shutter));
  } else {
    LOGF(ERROR) << "Invalid notify message type: " << msg->type;
  }

  return m;
}

}  // namespace arc
