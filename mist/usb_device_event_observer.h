// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MIST_USB_DEVICE_EVENT_OBSERVER_H_
#define MIST_USB_DEVICE_EVENT_OBSERVER_H_

#include <string>

#include <base/basictypes.h>

namespace mist {

// An interface class for observing USB device events from
// UsbDeviceEventNotifier.
class UsbDeviceEventObserver {
 public:
  // Invoked when a USB device is added to the system.
  virtual void OnUsbDeviceAdded(const std::string& syspath,
                                uint16 vendor_id, uint16 product_id) = 0;
  // Invoked when a USB device is removed from the system.
  virtual void OnUsbDeviceRemoved(const std::string& syspath) = 0;

 protected:
  UsbDeviceEventObserver() {}
  virtual ~UsbDeviceEventObserver() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(UsbDeviceEventObserver);
};

}  // namespace mist

#endif  // MIST_USB_DEVICE_EVENT_OBSERVER_H_
