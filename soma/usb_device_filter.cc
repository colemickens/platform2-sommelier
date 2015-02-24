// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "soma/usb_device_filter.h"

namespace soma {

USBDeviceFilter::~USBDeviceFilter() {}

USBDeviceFilter::USBDeviceFilter(int vid, int pid)
    : vid_(vid), pid_(pid) {
}

}  // namespace soma
