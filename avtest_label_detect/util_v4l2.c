// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <linux/videodev2.h>
#include <stdio.h>
#include <string.h>

#include "label_detect.h"

/* Returns true if device fd supports given format of buffer type.
 * Example of buf_type: V4L2_BUF_TYPE_VIDEO_OUTPUT,
 * V4L2_BUF_TYPE_VIDEO_CAPTURE.
 * */
bool is_v4l2_support_format(int fd, enum v4l2_buf_type buf_type,
    uint32_t fourcc) {
  int i;
  bool found = false;
  char fourcc_str[5];

  convert_fourcc_to_str(fourcc, fourcc_str);
  TRACE("is_v4l2_support_format(%s)\n", fourcc_str);
  for (i = 0; ; ++i) {
    struct v4l2_fmtdesc format_desc;
    memset(&format_desc, 0, sizeof(format_desc));
    format_desc.type = (enum v4l2_buf_type) buf_type;
    format_desc.index = i;
    if (-1 == do_ioctl(fd, VIDIOC_ENUM_FMT, &format_desc)) {
      break;
    }
    convert_fourcc_to_str(format_desc.pixelformat, fourcc_str);
    TRACE("%s supported\n", fourcc_str);
    if (format_desc.pixelformat == fourcc) {
      found = true;
      /* continue the loop in order to output all supported formats */
    }
  }
  TRACE("is_v4l2_support_format: %s\n", found ? "true" : "false");
  return found;
}

/* Returns true if device fd is V4L2 encode/decode device. */
bool is_hw_video_acc_device(int fd) {
  struct v4l2_capability cap;
  int ret = do_ioctl(fd, VIDIOC_QUERYCAP, &cap);
  if (ret == 0) {
    if ((cap.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE) &&
        (cap.capabilities & V4L2_CAP_VIDEO_OUTPUT_MPLANE) &&
        (cap.capabilities & V4L2_CAP_STREAMING)) {
      TRACE("is_hw_video_acc_device: true\n");
      return true;
    }
  }
  TRACE("is_hw_video_acc_device: false\n");
  return false;
}
