/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef USB_STREAM_FORMAT_H_
#define USB_STREAM_FORMAT_H_

#include <vector>

#include "usb/common_types.h"

namespace arc {

// Get the largest resolution from |supported_formats|.
SupportedFormat GetMaximumFormat(const SupportedFormats& supported_formats);

// Find all formats in preference order.
// The resolutions in returned SupportedFormats vector are unique.
SupportedFormats GetQualifiedFormats(const SupportedFormats& supported_formats);

}  // namespace arc

#endif  // USB_STREAM_FORMAT_H_
