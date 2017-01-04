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

// The caller must allocate the memory for out_handle.
int DeserializeNativeHandle(const arc::mojom::NativeHandlePtr& ptr,
                            native_handle_t* out_handle) {
  out_handle->version = ptr->version;
  out_handle->numFds = ptr->num_fds;
  out_handle->numInts = ptr->num_ints;
  for (int i = 0; i < ptr->num_fds; i++) {
    int unwrapped_handle = DeserializeHandle(ptr->fds[i]);
    if (unwrapped_handle == -EINVAL) {
      LOGF(ERROR) << "Failed to get native fd";
      for (int j = 0; j < i; j++) {
        close(out_handle->data[j]);
      }
      return -EINVAL;
    }
    out_handle->data[i] = unwrapped_handle;
  }
  for (int i = 0; i < ptr->num_ints; i++) {
    out_handle->data[ptr->num_fds + i] = ptr->ints[i];
  }
  return 0;
}

arc::mojom::Camera3StreamBufferPtr SerializeStreamBuffer(
    const camera3_stream_buffer_t* buffer,
    const UniqueStreams& streams,
    const std::unordered_map<buffer_handle_t, uint64_t>& buffer_handles) {
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

  if (buffer_handles.find(*buffer->buffer) == buffer_handles.end()) {
    LOGF(ERROR) << "Unknown buffer handle";
    ret.reset();
    return ret;
  }
  // Since we only need to return the handle IDs we can set all the other fields
  // in the buffer handle to 0.
  ret->buffer = arc::mojom::NativeHandle::New();
  ret->buffer->version = 0;
  ret->buffer->num_fds = 0;
  ret->buffer->num_ints = 0;
  ret->buffer->handle_id = buffer_handles.at(*buffer->buffer);

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

int DeserializeStreamBuffer(const arc::mojom::Camera3StreamBufferPtr& ptr,
                            const UniqueStreams& streams,
                            camera3_stream_buffer_t* out_buffer) {
  auto it = streams.find(ptr->stream_id);
  if (it == streams.end()) {
    LOGF(ERROR) << "Unknown stream: " << ptr->stream_id;
    return -EINVAL;
  }
  out_buffer->stream = it->second.get();

  int ret = DeserializeNativeHandle(
      ptr->buffer, *const_cast<native_handle**>(out_buffer->buffer));
  if (ret) {
    return ret;
  }
  out_buffer->status = ptr->status;

  int unwrapped_handle;
  unwrapped_handle = DeserializeHandle(ptr->acquire_fence);
  if (unwrapped_handle == -EINVAL) {
    LOGF(ERROR) << "Failed to get acquire_fence";
    native_handle_close(*(out_buffer->buffer));
    return -EINVAL;
  }
  out_buffer->acquire_fence = unwrapped_handle;

  unwrapped_handle = DeserializeHandle(ptr->release_fence);
  if (unwrapped_handle == -EINVAL) {
    LOGF(ERROR) << "Failed to get release_fence";
    native_handle_close(*(out_buffer->buffer));
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
