// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/vm/libvda/gbm_util.h"

#include <base/logging.h>

namespace arc {

uint32_t ConvertPixelFormatToGbmFormat(vda_pixel_format_t format) {
  switch (format) {
    case YV12:
      return GBM_FORMAT_YVU420;
    case NV12:
      return GBM_FORMAT_NV12;
    default:
      return 0;
  }
}

}  // namespace arc
