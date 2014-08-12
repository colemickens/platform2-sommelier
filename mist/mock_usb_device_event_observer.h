// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MIST_MOCK_USB_DEVICE_EVENT_OBSERVER_H_
#define MIST_MOCK_USB_DEVICE_EVENT_OBSERVER_H_

#include <string>

#include <base/basictypes.h>
#include <base/compiler_specific.h>
#include <gmock/gmock.h>

#include "mist/usb_device_event_observer.h"

namespace mist {

class MockUsbDeviceEventObserver : public UsbDeviceEventObserver {
 public:
  MockUsbDeviceEventObserver();
  ~MockUsbDeviceEventObserver() override;

  MOCK_METHOD5(OnUsbDeviceAdded,
               void(const std::string& sys_path,
                    uint8_t bus_number,
                    uint8_t device_address,
                    uint16_t vendor_id,
                    uint16_t product_id));
  MOCK_METHOD1(OnUsbDeviceRemoved, void(const std::string& sys_path));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockUsbDeviceEventObserver);
};

}  // namespace mist

#endif  // MIST_MOCK_USB_DEVICE_EVENT_OBSERVER_H_
