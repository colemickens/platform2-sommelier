/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hal_adapter/arc_camera3_mojo_utils.h"

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
    const std::unordered_map<uint64_t, std::unique_ptr<camera_buffer_handle_t>>&
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

  ret->status = static_cast<arc::mojom::Camera3BufferStatus>(buffer->status);

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
    const std::unordered_map<uint64_t, std::unique_ptr<camera_buffer_handle_t>>&
        buffer_handles,
    camera3_stream_buffer_t* out_buffer) {
  auto it = streams.find(ptr->stream_id);
  if (it == streams.end()) {
    LOGF(ERROR) << "Unknown stream: " << ptr->stream_id;
    return -EINVAL;
  }
  out_buffer->stream = it->second.get();

  auto buffer_handle = buffer_handles.find(ptr->buffer_id);
  if (buffer_handle == buffer_handles.end()) {
    LOGF(ERROR) << "Invalid buffer id: " << ptr->buffer_id;
    return -EINVAL;
  }
  *out_buffer->buffer =
      reinterpret_cast<buffer_handle_t>(buffer_handle->second.get());

  out_buffer->status = static_cast<int>(ptr->status);

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

const size_t CameraMetadataTypeSize[NUM_TYPES] =
    {[TYPE_BYTE] = sizeof(uint8_t),
     [TYPE_INT32] = sizeof(int32_t),
     [TYPE_FLOAT] = sizeof(float),
     [TYPE_INT64] = sizeof(int64_t),
     [TYPE_DOUBLE] = sizeof(double),
     [TYPE_RATIONAL] = sizeof(camera_metadata_rational_t)};

arc::mojom::CameraMetadataPtr SerializeCameraMetadata(
    const camera_metadata_t* metadata) {
  arc::mojom::CameraMetadataPtr result = arc::mojom::CameraMetadata::New();
  if (metadata) {
    result->size = get_camera_metadata_size(metadata);
    result->entry_count = get_camera_metadata_entry_count(metadata);
    result->entry_capacity = get_camera_metadata_entry_capacity(metadata);
    result->data_count = get_camera_metadata_data_count(metadata);
    result->data_capacity = get_camera_metadata_data_capacity(metadata);

    for (size_t i = 0; i < result->entry_count; ++i) {
      camera_metadata_ro_entry_t src;
      get_camera_metadata_ro_entry(metadata, i, &src);
      if (src.type >= NUM_TYPES) {
        LOGF(ERROR) << "Invalid camera metadata entry type: " << src.type;
        result.reset();
        return result;
      }

      arc::mojom::CameraMetadataEntryPtr dst =
          arc::mojom::CameraMetadataEntry::New();
      dst->index = src.index;
      dst->tag = static_cast<arc::mojom::CameraMetadataTag>(src.tag);
      dst->type = static_cast<arc::mojom::EntryType>(src.type);
      dst->count = src.count;
      size_t src_data_size = src.count * CameraMetadataTypeSize[src.type];
      std::vector<uint8_t> dst_data(src_data_size);
      memcpy(dst_data.data(), src.data.u8, src_data_size);
      dst->data = std::move(dst_data);
      result->entries.push_back(std::move(dst));
    }
    VLOGF(1) << "Serialized metadata size=" << result->size;
  }
  return result;
}

internal::CameraMetadataUniquePtr DeserializeCameraMetadata(
    const arc::mojom::CameraMetadataPtr& metadata) {
  internal::CameraMetadataUniquePtr result;
  if (!metadata->entries.is_null()) {
    camera_metadata_t* allocated_data = allocate_camera_metadata(
        metadata->entry_capacity, metadata->data_capacity);
    if (!allocated_data) {
      LOGF(ERROR) << "Failed to allocate camera metadata";
      return result;
    }
    result.reset(allocated_data);
    for (size_t i = 0; i < metadata->entry_count; ++i) {
      int ret = add_camera_metadata_entry(
          result.get(), static_cast<uint32_t>(metadata->entries[i]->tag),
          metadata->entries[i]->data.storage().data(),
          metadata->entries[i]->count);
      if (ret) {
        LOGF(ERROR) << "Failed to add camera metadata entry";
        result.reset();
        return result;
      }
    }
    VLOGF(1) << "Deserialized metadata size="
             << get_camera_metadata_size(result.get());
  }
  return result;
}
}  // namespace internal
