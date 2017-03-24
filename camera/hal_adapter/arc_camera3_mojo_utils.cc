/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hal_adapter/arc_camera3_mojo_utils.h"

#include <unordered_map>
#include <utility>
#include <vector>

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

  if (buffer->acquire_fence != -1) {
    ret->acquire_fence = WrapPlatformHandle(buffer->acquire_fence);
    if (!ret->acquire_fence.is_valid()) {
      LOGF(ERROR) << "Failed to wrap acquire_fence";
      ret.reset();
      return ret;
    }
  }

  if (buffer->release_fence != -1) {
    ret->release_fence = WrapPlatformHandle(buffer->release_fence);
    if (!ret->release_fence.is_valid()) {
      LOGF(ERROR) << "Failed to wrap release_fence";
      ret.reset();
      return ret;
    }
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
    LOGF(ERROR) << "Invalid buffer id: " << ptr->buffer_id;
    return -EINVAL;
  }
  *out_buffer->buffer =
      reinterpret_cast<buffer_handle_t>(buffer_handle->second.get());

  out_buffer->status = ptr->status;

  if (ptr->acquire_fence.is_valid()) {
    out_buffer->acquire_fence =
        UnwrapPlatformHandle(std::move(ptr->acquire_fence));
    if (out_buffer->acquire_fence == -EINVAL) {
      LOGF(ERROR) << "Failed to get acquire_fence";
      return -EINVAL;
    }
  } else {
    out_buffer->acquire_fence = -1;
  }

  if (ptr->release_fence.is_valid()) {
    out_buffer->release_fence =
        UnwrapPlatformHandle(std::move(ptr->release_fence));
    if (out_buffer->release_fence == -EINVAL) {
      LOGF(ERROR) << "Failed to get release_fence";
      close(out_buffer->acquire_fence);
      return -EINVAL;
    }
  } else {
    out_buffer->release_fence = -1;
  }

  return 0;
}

arc::mojom::CameraMetadataPtr SerializeCameraMetadata(
    const camera_metadata_t* metadata) {
  arc::mojom::CameraMetadataPtr result = arc::mojom::CameraMetadata::New();
  if (metadata) {
    size_t data_size = get_camera_metadata_size(metadata);
    std::vector<uint8_t> m(data_size);
    memcpy(m.data(), reinterpret_cast<const uint8_t*>(metadata), data_size);
    result->data = std::move(m);
    VLOGF(1) << "Serialized metadata size=" << data_size;
  }
  return result;
}

internal::CameraMetadataUniquePtr DeserializeCameraMetadata(
    const arc::mojom::CameraMetadataPtr& metadata) {
  internal::CameraMetadataUniquePtr result;
  if (!metadata->data.is_null()) {
    size_t data_size = metadata->data.size();
    uint8_t* data = new uint8_t[data_size];
    memcpy(data, metadata->data.storage().data(), data_size);
    result.reset(reinterpret_cast<camera_metadata_t*>(data));
    VLOGF(1) << "Deserialized metadata size=" << data_size;
  }
  return result;
}
}  // namespace internal
