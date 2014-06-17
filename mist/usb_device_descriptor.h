// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MIST_USB_DEVICE_DESCRIPTOR_H_
#define MIST_USB_DEVICE_DESCRIPTOR_H_

#include <ostream>  // NOLINT(readability/streams)
#include <string>

#include <base/basictypes.h>
#include <base/memory/weak_ptr.h>

struct libusb_device_descriptor;

namespace mist {

class UsbDevice;

// A USB device descriptor, which wraps a libusb_device_descriptor C struct from
// libusb 1.0 into a C++ object.
class UsbDeviceDescriptor {
 public:
  // Constructs a UsbDeviceDescriptor object by taking a weak pointer to a
  // UsbDevice object as |device| and a raw pointer to a
  // libusb_device_descriptor struct as |device_descriptor|. |device| is
  // used for getting USB string descriptors related to this object. The
  // ownership of |device_descriptor| is not transferred, and thus it should
  // outlive this object.
  UsbDeviceDescriptor(const base::WeakPtr<UsbDevice>& device,
                      const libusb_device_descriptor* device_descriptor);

  ~UsbDeviceDescriptor();

  // Getters for retrieving fields of the libusb_device_descriptor struct.
  uint8 GetLength() const;
  uint8 GetDescriptorType() const;
  uint8 GetDeviceClass() const;
  uint8 GetDeviceSubclass() const;
  uint8 GetDeviceProtocol() const;
  uint8 GetMaxPacketSize0() const;
  uint16 GetVendorId() const;
  uint16 GetProductId() const;
  std::string GetManufacturer() const;
  std::string GetProduct() const;
  std::string GetSerialNumber() const;
  uint8 GetNumConfigurations() const;

  // Returns a string describing the properties of this object for logging
  // purpose.
  std::string ToString() const;

 private:
  base::WeakPtr<UsbDevice> device_;
  const libusb_device_descriptor* const device_descriptor_;

  DISALLOW_COPY_AND_ASSIGN(UsbDeviceDescriptor);
};

}  // namespace mist

// Output stream operator provided to facilitate logging.
std::ostream& operator<<(std::ostream& stream,
                         const mist::UsbDeviceDescriptor& device_descriptor);

#endif  // MIST_USB_DEVICE_DESCRIPTOR_H_
