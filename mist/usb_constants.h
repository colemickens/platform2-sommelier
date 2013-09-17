// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MIST_USB_CONSTANTS_H_
#define MIST_USB_CONSTANTS_H_

#include <ostream>

#include <base/basictypes.h>

namespace mist {

// USB class codes.
enum UsbClass {
  kUsbClassMassStorage = 0x08
};

// USB endpoint direction, which is one-to-one equivalent to the
// libusb_endpoint_direction enum defined in libusb 1.0.
enum UsbDirection {
  // Device to host.
  kUsbDirectionIn = 0x80,
  // Host to device.
  kUsbDirectionOut = 0x00
};

// USB speed codes, which is one-to-one equivalent to the libusb_speed enum
// defined in libusb 1.0.
enum UsbSpeed {
  kUsbSpeedUnknown = 0,
  kUsbSpeedLow = 1,
  kUsbSpeedFull = 2,
  kUsbSpeedHigh = 3,
  kUsbSpeedSuper = 4
};

// USB endpoint transfer type, which is one-to-one equivalent to the
// libusb_transfer_type enum defined in libusb 1.0.
enum UsbTransferType {
  kUsbTransferTypeControl = 0,
  kUsbTransferTypeIsochronous = 1,
  kUsbTransferTypeBulk = 2,
  kUsbTransferTypeInterrupt = 3,
  // Additional enum value to indicate an uninitialized/unknown transfer type.
  kUsbTransferTypeUnknown = -1
};

// USB endpoint transfer status, which is one-to-one equivalent to the
// libusb_transfer_status enum defined in libusb 1.0.
enum UsbTransferStatus {
  kUsbTransferStatusCompleted,
  kUsbTransferStatusError,
  kUsbTransferStatusTimedOut,
  kUsbTransferStatusCancelled,
  kUsbTransferStatusStall,
  kUsbTransferStatusNoDevice,
  kUsbTransferStatusOverflow,
  // Additional enum value to indicate an unknown transfer status.
  kUsbTransferStatusUnknown
};

// Returns the USB endpoint direction of |endpoint_address|.
UsbDirection GetUsbDirectionOfEndpointAddress(uint8 endpoint_address);

// Returns a string describing the USB endpoint direction |direction|.
const char* UsbDirectionToString(UsbDirection direction);

// Returns a string describing the USB speed code |speed|.
const char* UsbSpeedToString(UsbSpeed speed);

// Returns a string describing the USB endpoint transfer type |transfer_type|.
const char* UsbTransferTypeToString(UsbTransferType transfer_type);

// Returns a string describing the USB endpoint transfer status
// |transfer_status|.
const char* UsbTransferStatusToString(UsbTransferStatus transfer_status);

}  // namespace mist

// Output stream operators provided to facilitate logging.
std::ostream& operator<<(std::ostream& stream, mist::UsbDirection direction);
std::ostream& operator<<(std::ostream& stream, mist::UsbSpeed speed);
std::ostream& operator<<(std::ostream& stream,
                         mist::UsbTransferType transfer_type);
std::ostream& operator<<(std::ostream& stream,
                         mist::UsbTransferStatus transfer_status);

#endif  // MIST_USB_CONSTANTS_H_
