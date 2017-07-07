/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef HAL_USB_FRAME_BUFFER_H_
#define HAL_USB_FRAME_BUFFER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include <arc/camera_buffer_mapper.h>
#include <base/files/scoped_file.h>
#include <base/synchronization/lock.h>
#include <camera/camera_metadata.h>

namespace arc {

class FrameBuffer {
 public:
  enum {
    YPLANE = 0,
    UPLANE = 1,
    VPLANE = 2,
  };

  FrameBuffer();
  virtual ~FrameBuffer();

  // If mapped successfully, the address will be assigned to |data_| and return
  // 0. Otherwise, returns -EINVAL.
  virtual int Map() = 0;

  // Unmaps the mapped address. Returns 0 for success.
  virtual int Unmap() = 0;

  uint8_t* GetData(size_t plane) const;
  uint8_t* GetData() const { return data_[0]; }
  size_t GetDataSize() const { return data_size_; }
  size_t GetBufferSize() const { return buffer_size_; }
  uint32_t GetWidth() const { return width_; }
  uint32_t GetHeight() const { return height_; }
  uint32_t GetFourcc() const { return fourcc_; }
  size_t GetStride(size_t plane) const;
  size_t GetStride() const { return stride_[0]; }

  void SetFourcc(uint32_t fourcc) { fourcc_ = fourcc; }
  virtual int SetDataSize(size_t data_size);

 protected:
  std::vector<uint8_t*> data_;
  std::vector<size_t> stride_;

  // The number of bytes used in the buffer.
  size_t data_size_;

  // The number of bytes allocated in the buffer.
  size_t buffer_size_;

  // Frame resolution.
  uint32_t width_;
  uint32_t height_;

  // This is V4L2_PIX_FMT_* in linux/videodev2.h.
  uint32_t fourcc_;

  // The number of planes.
  uint32_t num_planes_;
};

// AllocatedFrameBuffer is used for the buffer from hal malloc-ed. User should
// be aware to manage the memory.
class AllocatedFrameBuffer : public FrameBuffer {
 public:
  explicit AllocatedFrameBuffer(int buffer_size);
  ~AllocatedFrameBuffer() override;

  // No-op for the two functions.
  int Map() override { return 0; }
  int Unmap() override { return 0; }

  void SetWidth(uint32_t width) { width_ = width; }
  void SetHeight(uint32_t height) { height_ = height; }
  int SetDataSize(size_t data_size) override;

 private:
  std::unique_ptr<uint8_t[]> buffer_;
};

// V4L2FrameBuffer is used for the buffer from V4L2CameraDevice. Maps the fd
// in constructor. Unmaps and closes the fd in destructor.
class V4L2FrameBuffer : public FrameBuffer {
 public:
  V4L2FrameBuffer(base::ScopedFD fd,
                  int buffer_size,
                  uint32_t width,
                  uint32_t height,
                  uint32_t fourcc);
  // Unmaps |data_| and closes |fd_|.
  ~V4L2FrameBuffer();

  int Map() override;
  int Unmap() override;
  const int GetFd() const { return fd_.get(); }

 private:
  // File descriptor of V4L2 frame buffer.
  base::ScopedFD fd_;

  bool is_mapped_;

  // Lock to guard |is_mapped_|.
  base::Lock lock_;
};

// GrallocFrameBuffer is used for the buffer from Android framework. Uses
// CameraBufferMapper to lock and unlock the buffer.
class GrallocFrameBuffer : public FrameBuffer {
 public:
  // Fill |width_| and |height_| according to the parameters.
  GrallocFrameBuffer(buffer_handle_t buffer, uint32_t width, uint32_t height);
  ~GrallocFrameBuffer();

  // Fill |buffer_size_| and |data_|.
  int Map() override;
  int Unmap() override;

 private:
  // The currently used buffer for |buffer_mapper_| operations.
  buffer_handle_t buffer_;

  // Used to import gralloc buffer.
  CameraBufferMapper* buffer_mapper_;

  bool is_mapped_;

  // Lock to guard |is_mapped_|.
  base::Lock lock_;
};

}  // namespace arc

#endif  // HAL_USB_FRAME_BUFFER_H_
