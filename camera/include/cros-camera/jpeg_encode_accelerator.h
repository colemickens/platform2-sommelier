/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef INCLUDE_CROS_CAMERA_JPEG_ENCODE_ACCELERATOR_H_
#define INCLUDE_CROS_CAMERA_JPEG_ENCODE_ACCELERATOR_H_

#include <stdint.h>
#include <memory>

#include <base/bind.h>

namespace cros {

using EncodeWithFDCallback =
    base::Callback<void(int buffer_id, uint32_t output_size, int error)>;

// Encapsulates a converter from YU12 to JPEG format. This class is not
// thread-safe.
// Before using this class, make sure mojo is initialized first.
class JpegEncodeAccelerator {
 public:
  // Enumeration of encode errors.
  enum Status {
    ENCODE_OK,

    HW_JPEG_ENCODE_NOT_SUPPORTED,

    // Eg. creation of encoder thread failed.
    THREAD_CREATION_FAILED,

    // Invalid argument was passed to an API method, e.g. the format of
    // VideoFrame is not supported.
    INVALID_ARGUMENT,

    // Output buffer is inaccessible, e.g. failed to map on another process.
    INACCESSIBLE_OUTPUT_BUFFER,

    // Failed to parse the incoming YUV image.
    PARSE_IMAGE_FAILED,

    // A fatal failure occurred in the GPU process layer or one of its
    // dependencies. Examples of such failures include hardware failures,
    // driver failures, library failures, and so on.
    PLATFORM_FAILURE,

    // Largest used enum.
    LARGEST_GPU_ERROR_ENUM = PLATFORM_FAILURE,
    // The Mojo channel is corrupted. User can call Start() again to establish
    // the channel.
    TRY_START_AGAIN,
    // Create shared memory for input buffer failed.
    SHARED_MEMORY_FAIL,
    // mmap() for input failed.
    MMAP_FAIL,
    // No encode response from Mojo channel after timeout.
    NO_ENCODE_RESPONSE,
  };

  static std::unique_ptr<JpegEncodeAccelerator> CreateInstance();

  virtual ~JpegEncodeAccelerator() {}

  // Starts the Jpeg encoder. This method must be called before all
  // the other methods are called.
  // Returns true on success otherwise false.
  virtual bool Start() = 0;

  // Encodes the given buffer that contains one I420 image.
  // The image is encoded from memory of |input_fd| with size
  // |input_buffer_size|. The resolution of I420 image is |coded_size_width| and
  // |coded_size_height|. |exif_buffer| with |exif_buffer_size| will be inserted
  // into the encoded JPEG image.
  // Encoded JPEG image will be put onto memory associated with |output_fd|
  // with actual image size as |output_data_size|.
  //
  // Args:
  //    |input_fd|: A file descriptor of shared buffer.
  //    |input_buffer_size|: Size of input buffer.
  //    |coded_size_width|: Width of I420 image.
  //    |coded_size_height|: Height of I420 image.
  //    |exif_buffer|: Exif buffer that will be inserted into the encoded image.
  //    |exif_buffer_size|: Size of the Exif buffer.
  //    |output_fd|: A file descriptor of shared memory.
  //    |output_buffer_size|: Size of output buffer.
  //    |output_data_size|: Actual size of the encoded image. Will be filled by the
  //    method.
  //
  // Returns:
  //    Returns |status| to notify the encode status.
  //    If the return code is TRY_START_AGAIN, user can call Start() again and
  //    use this API.
  virtual int EncodeSync(int input_fd,
                         uint32_t input_buffer_size,
                         int32_t coded_size_width,
                         int32_t coded_size_height,
                         const uint8_t* exif_buffer,
                         uint32_t exif_buffer_size,
                         int output_fd,
                         uint32_t output_buffer_size,
                         uint32_t* output_data_size) = 0;
};

}  // namespace cros

#endif  // INCLUDE_CROS_CAMERA_JPEG_ENCODE_ACCELERATOR_H_
