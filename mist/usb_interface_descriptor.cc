// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mist/usb_interface_descriptor.h"

#include <libusb.h>

#include <base/logging.h>
#include <base/memory/scoped_ptr.h>
#include <base/stringprintf.h>

#include "mist/usb_device.h"
#include "mist/usb_endpoint_descriptor.h"

using base::StringPrintf;
using std::ostream;
using std::string;

namespace mist {

UsbInterfaceDescriptor::UsbInterfaceDescriptor(
    const base::WeakPtr<UsbDevice>& device,
    const libusb_interface_descriptor* interface_descriptor)
    : device_(device),
      interface_descriptor_(interface_descriptor) {
  CHECK(interface_descriptor_);
}

UsbInterfaceDescriptor::~UsbInterfaceDescriptor() {}

uint8 UsbInterfaceDescriptor::GetLength() const {
  return interface_descriptor_->bLength;
}

uint8 UsbInterfaceDescriptor::GetDescriptorType() const {
  return interface_descriptor_->bDescriptorType;
}

uint8 UsbInterfaceDescriptor::GetInterfaceNumber() const {
  return interface_descriptor_->bInterfaceNumber;
}

uint8 UsbInterfaceDescriptor::GetAlternateSetting() const {
  return interface_descriptor_->bAlternateSetting;
}

uint8 UsbInterfaceDescriptor::GetNumEndpoints() const {
  return interface_descriptor_->bNumEndpoints;
}

uint8 UsbInterfaceDescriptor::GetInterfaceClass() const {
  return interface_descriptor_->bInterfaceClass;
}

uint8 UsbInterfaceDescriptor::GetInterfaceSubclass() const {
  return interface_descriptor_->bInterfaceSubClass;
}

uint8 UsbInterfaceDescriptor::GetInterfaceProtocol() const {
  return interface_descriptor_->bInterfaceProtocol;
}

string UsbInterfaceDescriptor::GetInterfaceDescription() const {
  return device_ ?
      device_->GetStringDescriptorAscii(interface_descriptor_->iInterface) :
      string();
}

UsbEndpointDescriptor* UsbInterfaceDescriptor::GetEndpointDescriptor(
    uint8 index) const {
  if (index >= GetNumEndpoints()) {
    LOG(ERROR) << StringPrintf("Invalid endpoint index %d. "
                               "Must be less than %d.",
                               index, GetNumEndpoints());
    return NULL;
  }

  return new UsbEndpointDescriptor(&interface_descriptor_->endpoint[index]);
}

UsbEndpointDescriptor*
UsbInterfaceDescriptor::GetEndpointDescriptorByTransferTypeAndDirection(
    UsbTransferType transfer_type, UsbDirection direction) const {
  for (uint8 i = 0; i < GetNumEndpoints(); ++i) {
    scoped_ptr<UsbEndpointDescriptor> endpoint_descriptor(
        GetEndpointDescriptor(i));
    if ((endpoint_descriptor->GetTransferType() == transfer_type) &&
        (endpoint_descriptor->GetDirection() == direction)) {
      return endpoint_descriptor.release();
    }
  }
  return NULL;
}

string UsbInterfaceDescriptor::ToString() const {
  return StringPrintf("Interface (Length=%u, "
                      "DescriptorType=%u, "
                      "InterfaceNumber=%u, "
                      "AlternateSetting=%u, "
                      "NumEndpoints=%u, "
                      "InterfaceClass=%u, "
                      "InterfaceSubclass=%u, "
                      "InterfaceProtocol=%u, "
                      "Interface='%s')",
                      GetLength(),
                      GetDescriptorType(),
                      GetInterfaceNumber(),
                      GetAlternateSetting(),
                      GetNumEndpoints(),
                      GetInterfaceClass(),
                      GetInterfaceSubclass(),
                      GetInterfaceProtocol(),
                      GetInterfaceDescription().c_str());
}

}  // namespace mist

ostream& operator<<(ostream& stream,
                    const mist::UsbInterfaceDescriptor& interface_descriptor) {
  stream << interface_descriptor.ToString();
  return stream;
}
