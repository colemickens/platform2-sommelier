// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mist/usb_bulk_transfer.h"

#include <libusb.h>

#include "mist/usb_device.h"

namespace mist {

bool UsbBulkTransfer::Initialize(const UsbDevice& device,
                                 uint8_t endpoint_address,
                                 int length,
                                 uint32_t timeout) {
  if (!device.IsOpen()) {
    mutable_error()->set_type(UsbError::kErrorDeviceNotOpen);
    return false;
  }

  // Allocate no isochronous packet descriptors.
  if (!Allocate(0) || !AllocateBuffer(length))
    return false;

  libusb_fill_bulk_transfer(transfer(),
                            device.device_handle(),
                            endpoint_address,
                            buffer(),
                            length,
                            &UsbTransfer::OnCompleted,
                            this,
                            timeout);
  return true;
}

}  // namespace mist
