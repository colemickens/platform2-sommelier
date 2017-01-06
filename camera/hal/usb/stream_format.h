/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef HAL_USB_STREAM_FORMAT_H_
#define HAL_USB_STREAM_FORMAT_H_

#include <vector>

#include "hal/usb/common_types.h"

namespace arc {

// Find a resolution from a supported list.
const SupportedFormat* FindFormatByResolution(const SupportedFormats& formats,
                                              uint32_t width,
                                              uint32_t height);

// Get the largest resolution from |supported_formats|.
SupportedFormat GetMaximumFormat(const SupportedFormats& supported_formats);

// Find all formats in preference order.
// The resolutions in returned SupportedFormats vector are unique.
SupportedFormats GetQualifiedFormats(const SupportedFormats& supported_formats);

// Get corresponding FOURCC from HAL_PIXEL_FORMAT.
int HalPixelFormatToFourcc(uint32_t hal_pixel_format);

}  // namespace arc

#endif  // HAL_USB_STREAM_FORMAT_H_
