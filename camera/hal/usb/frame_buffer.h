/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef HAL_USB_FRAME_BUFFER_H_
#define HAL_USB_FRAME_BUFFER_H_

#include <stdint.h>

#include <string>

namespace arc {

struct FrameBuffer {
  uint8_t* data;
  size_t data_size; /* How many bytes used in the buffer */
  size_t buffer_size; /* How many bytes allocated in the buffer */
  uint32_t width;
  uint32_t height;
  uint32_t fourcc; /* This is V4L2_PIX_FMT_* in linux/videodev2.h */
};

}  // namespace arc

#endif  // HAL_USB_FRAME_BUFFER_H_
