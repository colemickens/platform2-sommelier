// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mist/usb_endpoint_descriptor.h"

#include <libusb.h>

#include <base/logging.h>
#include <base/stringprintf.h>

using base::StringPrintf;
using std::ostream;
using std::string;

namespace mist {

UsbEndpointDescriptor::UsbEndpointDescriptor(
    const libusb_endpoint_descriptor* endpoint_descriptor)
    : endpoint_descriptor_(endpoint_descriptor) {
  CHECK(endpoint_descriptor_);
}

UsbEndpointDescriptor::~UsbEndpointDescriptor() {}

uint8 UsbEndpointDescriptor::GetLength() const {
  return endpoint_descriptor_->bLength;
}

uint8 UsbEndpointDescriptor::GetDescriptorType() const {
  return endpoint_descriptor_->bDescriptorType;
}

uint8 UsbEndpointDescriptor::GetEndpointAddress() const {
  return endpoint_descriptor_->bEndpointAddress;
}

uint8 UsbEndpointDescriptor::GetEndpointNumber() const {
  return GetEndpointAddress() & LIBUSB_ENDPOINT_ADDRESS_MASK;
}

uint8 UsbEndpointDescriptor::GetAttributes() const {
  return endpoint_descriptor_->bmAttributes;
}

uint16 UsbEndpointDescriptor::GetMaxPacketSize() const {
  return endpoint_descriptor_->wMaxPacketSize;
}

uint8 UsbEndpointDescriptor::GetInterval() const {
  return endpoint_descriptor_->bInterval;
}

UsbDirection UsbEndpointDescriptor::GetDirection() const {
  uint8 direction = GetEndpointAddress() & LIBUSB_ENDPOINT_DIR_MASK;
  return (direction == LIBUSB_ENDPOINT_IN) ? kUsbDirectionIn : kUsbDirectionOut;
}

UsbTransferType UsbEndpointDescriptor::GetTransferType() const {
  return static_cast<UsbTransferType>(GetAttributes() &
                                      LIBUSB_TRANSFER_TYPE_MASK);
}

string UsbEndpointDescriptor::ToString() const {
  return StringPrintf("Endpoint (Length=%u, "
                      "DescriptorType=%u, "
                      "EndpointAddress=0x%02x, "
                      "EndpointNumber=%u, "
                      "Attributes=0x%02x, "
                      "MaxPacketSize=%u, "
                      "Interval=%u, "
                      "Direction=%s, "
                      "TransferType=%s)",
                      GetLength(),
                      GetDescriptorType(),
                      GetEndpointAddress(),
                      GetEndpointNumber(),
                      GetAttributes(),
                      GetMaxPacketSize(),
                      GetInterval(),
                      UsbDirectionToString(GetDirection()),
                      UsbTransferTypeToString(GetTransferType()));
}

}  // namespace mist

ostream& operator<<(ostream& stream,
                    const mist::UsbEndpointDescriptor& endpoint_descriptor) {
  stream << endpoint_descriptor.ToString();
  return stream;
}
