// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mist/usb_device_descriptor.h"

#include <libusb.h>

#include <base/logging.h>
#include <base/strings/stringprintf.h>

#include "mist/usb_device.h"

using base::StringPrintf;
using std::ostream;
using std::string;

namespace mist {

UsbDeviceDescriptor::UsbDeviceDescriptor(
    const base::WeakPtr<UsbDevice>& device,
    const libusb_device_descriptor* device_descriptor)
    : device_(device),
      device_descriptor_(device_descriptor) {
  CHECK(device_descriptor_);
}

UsbDeviceDescriptor::~UsbDeviceDescriptor() {}

uint8 UsbDeviceDescriptor::GetLength() const {
  return device_descriptor_->bLength;
}

uint8 UsbDeviceDescriptor::GetDescriptorType() const {
  return device_descriptor_->bDescriptorType;
}

uint8 UsbDeviceDescriptor::GetDeviceClass() const {
  return device_descriptor_->bDeviceClass;
}

uint8 UsbDeviceDescriptor::GetDeviceSubclass() const {
  return device_descriptor_->bDeviceSubClass;
}

uint8 UsbDeviceDescriptor::GetDeviceProtocol() const {
  return device_descriptor_->bDeviceProtocol;
}

uint8 UsbDeviceDescriptor::GetMaxPacketSize0() const {
  return device_descriptor_->bMaxPacketSize0;
}

uint16 UsbDeviceDescriptor::GetVendorId() const {
  return device_descriptor_->idVendor;
}

uint16 UsbDeviceDescriptor::GetProductId() const {
  return device_descriptor_->idProduct;
}

string UsbDeviceDescriptor::GetManufacturer() const {
  return device_ ?
      device_->GetStringDescriptorAscii(device_descriptor_->iManufacturer) :
      string();
}

string UsbDeviceDescriptor::GetProduct() const {
  return device_ ?
      device_->GetStringDescriptorAscii(device_descriptor_->iProduct) :
      string();
}

string UsbDeviceDescriptor::GetSerialNumber() const {
  return device_ ?
      device_->GetStringDescriptorAscii(device_descriptor_->iSerialNumber) :
      string();
}

uint8 UsbDeviceDescriptor::GetNumConfigurations() const {
  return device_descriptor_->bNumConfigurations;
}

string UsbDeviceDescriptor::ToString() const {
  return StringPrintf("Device (Length=%u, "
                      "DescriptorType=%u, "
                      "DeviceClass=%u, "
                      "DeviceSubclass=%u, "
                      "DeviceProtocol=%u, "
                      "MaxPacketSize0=%u, "
                      "VendorId=0x%04x, "
                      "ProductId=0x%04x, "
                      "Manufacturer='%s', "
                      "Product='%s', "
                      "SerialNumber='%s', "
                      "NumConfigurations=%d)",
                      GetLength(),
                      GetDescriptorType(),
                      GetDeviceClass(),
                      GetDeviceSubclass(),
                      GetDeviceProtocol(),
                      GetMaxPacketSize0(),
                      GetVendorId(),
                      GetProductId(),
                      GetManufacturer().c_str(),
                      GetProduct().c_str(),
                      GetSerialNumber().c_str(),
                      GetNumConfigurations());
}

}  // namespace mist

ostream& operator<<(ostream& stream,
                    const mist::UsbDeviceDescriptor& device_descriptor) {
  stream << device_descriptor.ToString();
  return stream;
}
