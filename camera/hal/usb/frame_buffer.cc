/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hal/usb/frame_buffer.h"

#include <sys/mman.h>

#include <utility>

#include "arc/common.h"
#include "hal/usb/image_processor.h"

namespace arc {

FrameBuffer::FrameBuffer()
    : data_(nullptr),
      data_size_(0),
      buffer_size_(0),
      width_(0),
      height_(0),
      fourcc_(0) {}

FrameBuffer::~FrameBuffer() {}

int FrameBuffer::SetDataSize(size_t data_size) {
  if (data_size > buffer_size_) {
    LOGF(ERROR) << "Buffer overflow: Buffer only has " << buffer_size_
                << ", but data needs " << data_size;
    return -EINVAL;
  }
  data_size_ = data_size;
  return 0;
}

AllocatedFrameBuffer::AllocatedFrameBuffer(int buffer_size) {
  buffer_.reset(new uint8_t[buffer_size]);
  buffer_size_ = buffer_size;
  data_ = buffer_.get();
}

AllocatedFrameBuffer::~AllocatedFrameBuffer() {}

int AllocatedFrameBuffer::SetDataSize(size_t size) {
  if (size > buffer_size_) {
    buffer_.reset(new uint8_t[size]);
    buffer_size_ = size;
    data_ = buffer_.get();
  }
  data_size_ = size;
  return 0;
}

V4L2FrameBuffer::V4L2FrameBuffer(base::ScopedFD fd,
                                 int buffer_size,
                                 uint32_t width,
                                 uint32_t height,
                                 uint32_t fourcc)
    : fd_(std::move(fd)), is_mapped_(false) {
  buffer_size_ = buffer_size;
  width_ = width;
  height_ = height;
  fourcc_ = fourcc;
}

V4L2FrameBuffer::~V4L2FrameBuffer() {
  if (Unmap()) {
    LOGF(ERROR) << "Unmap failed";
  }
}

int V4L2FrameBuffer::Map() {
  base::AutoLock l(lock_);
  if (is_mapped_) {
    LOGF(ERROR) << "The buffer is already mapped";
    return -EINVAL;
  }
  void* addr = mmap(NULL, buffer_size_, PROT_READ, MAP_SHARED, fd_.get(), 0);
  if (addr == MAP_FAILED) {
    LOGF(ERROR) << "mmap() failed: " << strerror(errno);
    return -EINVAL;
  }
  data_ = static_cast<uint8_t*>(addr);
  is_mapped_ = true;
  return 0;
}

int V4L2FrameBuffer::Unmap() {
  base::AutoLock l(lock_);
  if (is_mapped_ && munmap(data_, buffer_size_)) {
    LOGF(ERROR) << "mummap() failed: " << strerror(errno);
    return -EINVAL;
  }
  is_mapped_ = false;
  return 0;
}

GrallocFrameBuffer::GrallocFrameBuffer(buffer_handle_t buffer,
                                       uint32_t width,
                                       uint32_t height,
                                       uint32_t fourcc,
                                       const CameraMetadata& metadata)
    : buffer_(buffer),
      buffer_mapper_(CameraBufferMapper::GetInstance()),
      jpeg_max_size_(0),
      is_mapped_(false) {
  int ret = buffer_mapper_->Register(buffer_);
  if (ret) {
    LOGF(ERROR) << "Failed to register buffer";
    return;
  }
  width_ = width;
  height_ = height;
  fourcc_ = fourcc;

  if (metadata.Exists(ANDROID_JPEG_MAX_SIZE)) {
    camera_metadata_ro_entry entry = metadata.Find(ANDROID_JPEG_MAX_SIZE);
    jpeg_max_size_ = entry.data.i32[0];
  }
}

GrallocFrameBuffer::~GrallocFrameBuffer() {
  if (Unmap()) {
    LOGF(ERROR) << "Unmap failed";
  }

  int ret = buffer_mapper_->Deregister(buffer_);
  if (ret) {
    LOGF(ERROR) << "Failed to unregister buffer";
  }
}

int GrallocFrameBuffer::Map() {
  base::AutoLock l(lock_);
  if (is_mapped_) {
    LOGF(ERROR) << "The buffer is already mapped";
    return -EINVAL;
  }

  void* addr;
  int ret;
  switch (fourcc_) {
    case V4L2_PIX_FMT_JPEG:
      if (!jpeg_max_size_) {
        LOGF(ERROR) << "JPEG_MAX_SIZE is undefined.";
        return -EINVAL;
      }
      buffer_size_ = jpeg_max_size_;
      ret = buffer_mapper_->Lock(buffer_, 0, 0, 0, jpeg_max_size_, 1, &addr);
      break;
    case V4L2_PIX_FMT_RGB32:
      // We don't know the actually buffer size from framework, calculate it by
      // attributes.
      buffer_size_ = ImageProcessor::GetConvertedSize(fourcc_, width_, height_);
      ret = buffer_mapper_->Lock(buffer_, 0, 0, 0, width_, height_, &addr);
      break;
    default:
      LOGF(ERROR) << "Format " << FormatToString(fourcc_) << " is unsupported";
      return -EINVAL;
  }
  if (ret) {
    LOGF(ERROR) << "Failed to map buffer";
    return -EINVAL;
  }

  data_ = static_cast<uint8_t*>(addr);
  is_mapped_ = true;
  return 0;
}

int GrallocFrameBuffer::Unmap() {
  base::AutoLock l(lock_);
  if (is_mapped_ && buffer_mapper_->Unlock(buffer_)) {
    LOGF(ERROR) << "Failed to unmap buffer";
    return -EINVAL;
  }
  is_mapped_ = false;
  return 0;
}

}  // namespace arc
