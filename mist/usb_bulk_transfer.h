// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MIST_USB_BULK_TRANSFER_H_
#define MIST_USB_BULK_TRANSFER_H_

#include <base/basictypes.h>
#include <base/compiler_specific.h>

#include "mist/usb_transfer.h"

namespace mist {

class UsbDevice;

// A USB bulk transfer, which extends UsbTransfer.
class UsbBulkTransfer : public UsbTransfer {
 public:
  UsbBulkTransfer();
  ~UsbBulkTransfer();

  // Initializes this USB bulk transfer for the specified |endpoint_address| on
  // |device| with a transfer buffer size of |length| bytes and a timeout value
  // of |timeout| seconds. Returns true on success. If |device| is not open,
  // sets |error_| to UsbError::kErrorDeviceNotOpen and returns false.
  bool Initialize(const UsbDevice& device,
                  uint8 endpoint_address,
                  int length,
                  uint32 timeout);

 private:
  DISALLOW_COPY_AND_ASSIGN(UsbBulkTransfer);
};

}  // namespace mist

#endif  // MIST_USB_BULK_TRANSFER_H_
