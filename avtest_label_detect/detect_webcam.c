// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <linux/videodev2.h>
#include <stdbool.h>

#include "label_detect.h"

/* For a given /dev/video* device, returns true if it is camera device. That
 * is, it has CAPTURE capability but no OUTPUT capability (otherwise, it may be
 * a transcoding device.)
 */
bool is_webcam_device(int fd) {
  struct v4l2_capability cap;
  int ret = do_ioctl(fd, VIDIOC_QUERYCAP, &cap);
  if (ret == 0) {
    if ((cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) &&
        !(cap.capabilities & V4L2_CAP_VIDEO_OUTPUT))
      return true;
  }
  return false;
}

/* Determines "webcam" label. That is, there is a /dev/video* device has
 * capabilities like webcam.
 */
bool detect_webcam(void) {
  static const char kVideoDeviceName[] = "/dev/video*";
  return is_any_device(kVideoDeviceName, is_webcam_device);
}
