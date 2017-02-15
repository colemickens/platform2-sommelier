/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hal_adapter/arc_camera3_mojo_utils.h"

#include <unordered_map>
#include <utility>

#include <mojo/edk/embedder/embedder.h>
#include <mojo/edk/embedder/scoped_platform_handle.h>
#include <mojo/edk/system/handle_signals_state.h>

namespace internal {

mojo::ScopedHandle WrapPlatformHandle(int handle) {
  MojoHandle wrapped_handle;
  MojoResult wrap_result = mojo::edk::CreatePlatformHandleWrapper(
      mojo::edk::ScopedPlatformHandle(mojo::edk::PlatformHandle(handle)),
      &wrapped_handle);
  if (wrap_result != MOJO_RESULT_OK) {
    LOGF(ERROR) << "Failed to wrap platform handle: " << wrap_result;
    return mojo::ScopedHandle(mojo::Handle());
  }
  return mojo::ScopedHandle(mojo::Handle(wrapped_handle));
}

// Transfers ownership of the handle.
int UnwrapPlatformHandle(mojo::ScopedHandle handle) {
  mojo::edk::ScopedPlatformHandle scoped_platform_handle;
  MojoResult mojo_result = mojo::edk::PassWrappedPlatformHandle(
      handle.release().value(), &scoped_platform_handle);
  if (mojo_result != MOJO_RESULT_OK) {
    LOGF(ERROR) << "Failed to unwrap handle: " << mojo_result;
    return -EINVAL;
  }
  return scoped_platform_handle.release().handle;
}

arc::mojom::HandlePtr SerializeHandle(int handle) {
  arc::mojom::HandlePtr ret = arc::mojom::Handle::New();
  if (handle == -1) {
    ret->set_none(true);
  } else if (handle >= 0) {
    ret->set_h(WrapPlatformHandle(handle));
  } else {
    LOGF(ERROR) << "Invalid handle to wrap";
    // Simply return an invalid handle to indicate that an error has occurred.
    ret->set_h(mojo::ScopedHandle(mojo::Handle()));
  }
  return ret;
}

// Transfers ownership of the handle.
int DeserializeHandle(const arc::mojom::HandlePtr& handle) {
  if (handle->is_none()) {
    return -1;
  }
  return UnwrapPlatformHandle(std::move(handle->get_h()));
}

arc::mojom::Camera3StreamBufferPtr SerializeStreamBuffer(
    const camera3_stream_buffer_t* buffer,
    const UniqueStreams& streams,
    const std::unordered_map<uint64_t,
                             internal::ArcCameraBufferHandleUniquePtr>&
        buffer_handles) {
  arc::mojom::Camera3StreamBufferPtr ret =
      arc::mojom::Camera3StreamBuffer::New();

  if (!buffer) {
    ret.reset();
    return ret;
  }

  auto it = streams.begin();
  for (; it != streams.end(); it++) {
    if (it->second.get() == buffer->stream) {
      break;
    }
  }
  if (it == streams.end()) {
    LOGF(ERROR) << "Unknown stream set in buffer";
    ret.reset();
    return ret;
  }
  ret->stream_id = it->first;

  auto handle = camera_buffer_handle_t::FromBufferHandle(*buffer->buffer);
  if (!handle) {
    ret.reset();
    return ret;
  }
  if (buffer_handles.find(handle->buffer_id) == buffer_handles.end()) {
    LOGF(ERROR) << "Unknown buffer handle";
    ret.reset();
    return ret;
  }
  ret->buffer_id = handle->buffer_id;

  ret->status = buffer->status;

  ret->acquire_fence = SerializeHandle(buffer->acquire_fence);
  if (ret->acquire_fence->is_h() && !ret->acquire_fence->get_h().is_valid()) {
    LOGF(ERROR) << "Failed to wrap acquire_fence";
    ret.reset();
    return ret;
  }

  ret->release_fence = SerializeHandle(buffer->release_fence);
  if (ret->release_fence->is_h() && !ret->release_fence->get_h().is_valid()) {
    LOGF(ERROR) << "Failed to wrap release_fence";
    ret.reset();
    return ret;
  }

  return ret;
}

int DeserializeStreamBuffer(
    const arc::mojom::Camera3StreamBufferPtr& ptr,
    const UniqueStreams& streams,
    const std::unordered_map<uint64_t,
                             internal::ArcCameraBufferHandleUniquePtr>&
        buffer_handles_,
    camera3_stream_buffer_t* out_buffer) {
  auto it = streams.find(ptr->stream_id);
  if (it == streams.end()) {
    LOGF(ERROR) << "Unknown stream: " << ptr->stream_id;
    return -EINVAL;
  }
  out_buffer->stream = it->second.get();

  auto buffer_handle = buffer_handles_.find(ptr->buffer_id);
  if (buffer_handle == buffer_handles_.end()) {
    LOG(ERROR) << "Invalid buffer id: " << ptr->buffer_id;
    return -EINVAL;
  }
  *out_buffer->buffer =
      reinterpret_cast<buffer_handle_t>(buffer_handle->second.get());

  out_buffer->status = ptr->status;

  int unwrapped_handle;
  unwrapped_handle = DeserializeHandle(ptr->acquire_fence);
  if (unwrapped_handle == -EINVAL) {
    LOGF(ERROR) << "Failed to get acquire_fence";
    return -EINVAL;
  }
  out_buffer->acquire_fence = unwrapped_handle;

  unwrapped_handle = DeserializeHandle(ptr->release_fence);
  if (unwrapped_handle == -EINVAL) {
    LOGF(ERROR) << "Failed to get release_fence";
    close(out_buffer->acquire_fence);
    return -EINVAL;
  }
  out_buffer->release_fence = unwrapped_handle;

  return 0;
}

int32_t SerializeCameraMetadata(
    mojo::ScopedDataPipeProducerHandle* producer_handle,
    mojo::ScopedDataPipeConsumerHandle* consumer_handle,
    const camera_metadata_t* metadata) {
  if (!metadata) {
    return -EINVAL;
  }
  // Serialize camera metadata.
  uint32_t data_size = get_camera_metadata_size(metadata);
  VLOGF(1) << "Camera metadata size: " << data_size;
  struct MojoCreateDataPipeOptions options = {
      sizeof(struct MojoCreateDataPipeOptions),
      MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE, 1, data_size};
  mojo::CreateDataPipe(&options, producer_handle, consumer_handle);
  mojo::WriteDataRaw(producer_handle->get(), metadata, &data_size,
                     MOJO_WRITE_DATA_FLAG_ALL_OR_NONE);
  if (!data_size) {
    LOGF(ERROR) << "Failed to write camera metadata through data pipe";
    return -EIO;
  }
  VLOGF(1) << "Data written to pipe: " << data_size;
  return 0;
}

CameraMetadataUniquePtr DeserializeCameraMetadata(
    mojo::DataPipeConsumerHandle consumer_handle) {
  uint32_t data_size = 0;
  mojo::edk::HandleSignalsState state;
  MojoWait(consumer_handle.value(), MOJO_HANDLE_SIGNAL_READABLE,
           MOJO_DEADLINE_INDEFINITE, &state);
  DCHECK(MOJO_HANDLE_SIGNAL_READABLE == state.satisfied_signals);

  mojo::ReadDataRaw(consumer_handle, nullptr, &data_size,
                    MOJO_READ_DATA_FLAG_QUERY);
  VLOGF(1) << "Data size in pipe: " << data_size;

  camera_metadata_t* metadata =
      reinterpret_cast<camera_metadata_t*>(new uint8_t[data_size]);
  mojo::ReadDataRaw(consumer_handle, metadata, &data_size,
                    MOJO_READ_DATA_FLAG_ALL_OR_NONE);
  if (!data_size) {
    LOGF(ERROR) << "Failed to read camera metadata from data pipe";
    // Simply return an invalid pointer to indicate that an error has occurred.
    return CameraMetadataUniquePtr();
  }
  VLOGF(1) << "Metadata size=" << get_camera_metadata_size(metadata);
  return CameraMetadataUniquePtr(metadata);
}

}  // namespace internal
