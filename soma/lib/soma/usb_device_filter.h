// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SOMA_LIB_SOMA_USB_DEVICE_FILTER_H_
#define SOMA_LIB_SOMA_USB_DEVICE_FILTER_H_

#include <base/basictypes.h>

namespace soma {

class USBDeviceFilter {
 public:
  USBDeviceFilter(int vid, int pid);
  virtual ~USBDeviceFilter();

  // TODO(cmasone): handle wildcarding in both major and minor.
  bool Allows(const USBDeviceFilter& rhs) {
    return vid_ == rhs.vid_ && pid_ == rhs.pid_;
  }

 private:
  int vid_;
  int pid_;
  DISALLOW_COPY_AND_ASSIGN(USBDeviceFilter);
};

}  // namespace soma
#endif  // SOMA_LIB_SOMA_USB_DEVICE_FILTER_H_
