// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mist/usb_device.h"

#include <libusb.h>

#include <base/logging.h>

#include "mist/usb_config_descriptor.h"
#include "mist/usb_device_descriptor.h"

using std::string;

namespace mist {

UsbDevice::UsbDevice(libusb_device* device)
    : device_(device),
      device_handle_(NULL) {
  CHECK(device_);
  libusb_ref_device(device_);
}

UsbDevice::UsbDevice(libusb_device_handle* device_handle)
    : device_(NULL),
      device_handle_(device_handle) {
  CHECK(device_handle_);
  device_ = libusb_get_device(device_handle_);
  CHECK(device_);
  libusb_ref_device(device_);
}

UsbDevice::~UsbDevice() {
  Close();
  libusb_unref_device(device_);
  device_ = NULL;
}

bool UsbDevice::IsOpen() const {
  return device_handle_ != NULL;
}

bool UsbDevice::VerifyOpen() {
  if (IsOpen())
    return true;

  error_.set_type(UsbError::kErrorDeviceNotOpen);
  return false;
}

bool UsbDevice::Open() {
  if (IsOpen()) {
    error_.Clear();
    return true;
  }

  int result = libusb_open(device_, &device_handle_);
  return error_.SetFromLibUsbError(static_cast<libusb_error>(result));
}

void UsbDevice::Close() {
  if (!IsOpen())
    return;

  libusb_close(device_handle_);
  device_handle_ = NULL;
}

bool UsbDevice::Reset() {
  if (!VerifyOpen())
    return false;

  int result = libusb_reset_device(device_handle_);
  return error_.SetFromLibUsbError(static_cast<libusb_error>(result));
}

uint8 UsbDevice::GetBusNumber() const {
  return libusb_get_bus_number(device_);
}

uint8 UsbDevice::GetDeviceAddress() const {
  return libusb_get_device_address(device_);
}

UsbSpeed UsbDevice::GetDeviceSpeed() const {
  int speed = libusb_get_device_speed(device_);
  switch (speed) {
    case kUsbSpeedLow:
    case kUsbSpeedFull:
    case kUsbSpeedHigh:
    case kUsbSpeedSuper:
      return static_cast<UsbSpeed>(speed);
    default:
      return kUsbSpeedUnknown;
  }
}

bool UsbDevice::GetConfiguration(int* configuration) {
  if (!VerifyOpen())
    return false;

  int result = libusb_get_configuration(device_handle_, configuration);
  return error_.SetFromLibUsbError(static_cast<libusb_error>(result));
}

bool UsbDevice::SetConfiguration(int configuration) {
  if (!VerifyOpen())
    return false;

  int result = libusb_set_configuration(device_handle_, configuration);
  return error_.SetFromLibUsbError(static_cast<libusb_error>(result));
}

bool UsbDevice::ClaimInterface(int interface_number) {
  if (!VerifyOpen())
    return false;

  int result = libusb_claim_interface(device_handle_, interface_number);
  return error_.SetFromLibUsbError(static_cast<libusb_error>(result));
}

bool UsbDevice::ReleaseInterface(int interface_number) {
  if (!VerifyOpen())
    return false;

  int result = libusb_release_interface(device_handle_, interface_number);
  return error_.SetFromLibUsbError(static_cast<libusb_error>(result));
}

bool UsbDevice::SetInterfaceAlternateSetting(int interface_number,
                                             int alternate_setting) {
  if (!VerifyOpen())
    return false;

  int result = libusb_set_interface_alt_setting(
      device_handle_, interface_number, alternate_setting);
  return error_.SetFromLibUsbError(static_cast<libusb_error>(result));
}

bool UsbDevice::IsKernelDriverActive(int interface_number) {
  if (!VerifyOpen())
    return false;

  int result = libusb_kernel_driver_active(device_handle_, interface_number);
  if (result == 1) {
    error_.Clear();
    return true;
  }

  return error_.SetFromLibUsbError(static_cast<libusb_error>(result));
}

bool UsbDevice::AttachKernelDriver(int interface_number) {
  if (!VerifyOpen())
    return false;

  int result = libusb_attach_kernel_driver(device_handle_, interface_number);
  return error_.SetFromLibUsbError(static_cast<libusb_error>(result));
}

bool UsbDevice::DetachKernelDriver(int interface_number) {
  if (!VerifyOpen())
    return false;

  int result = libusb_detach_kernel_driver(device_handle_, interface_number);
  return error_.SetFromLibUsbError(static_cast<libusb_error>(result));
}

bool UsbDevice::ClearHalt(uint8 endpoint) {
  if (!VerifyOpen())
    return false;

  int result = libusb_clear_halt(device_handle_, endpoint);
  return error_.SetFromLibUsbError(static_cast<libusb_error>(result));
}

UsbConfigDescriptor* UsbDevice::GetActiveConfigDescriptor() {
  libusb_config_descriptor* config_descriptor = NULL;

  int result = libusb_get_active_config_descriptor(device_, &config_descriptor);
  if (error_.SetFromLibUsbError(static_cast<libusb_error>(result)))
    return new UsbConfigDescriptor(AsWeakPtr(), config_descriptor, true);

  return NULL;
}

UsbConfigDescriptor* UsbDevice::GetConfigDescriptor(uint8 index) {
  libusb_config_descriptor* config_descriptor = NULL;

  int result = libusb_get_config_descriptor(device_, index, &config_descriptor);
  if (error_.SetFromLibUsbError(static_cast<libusb_error>(result)))
    return new UsbConfigDescriptor(AsWeakPtr(), config_descriptor, true);

  return NULL;
}

UsbConfigDescriptor* UsbDevice::GetConfigDescriptorByValue(
    uint8 configuration_value) {
  libusb_config_descriptor* config_descriptor = NULL;

  int result = libusb_get_config_descriptor_by_value(
      device_, configuration_value, &config_descriptor);
  if (error_.SetFromLibUsbError(static_cast<libusb_error>(result)))
    return new UsbConfigDescriptor(AsWeakPtr(), config_descriptor, true);

  return NULL;
}

UsbDeviceDescriptor* UsbDevice::GetDeviceDescriptor() {
  if (!device_descriptor_)
    device_descriptor_.reset(new libusb_device_descriptor());

  int result = libusb_get_device_descriptor(device_, device_descriptor_.get());
  if (error_.SetFromLibUsbError(static_cast<libusb_error>(result)))
    return new UsbDeviceDescriptor(AsWeakPtr(), device_descriptor_.get());

  return NULL;
}

string UsbDevice::GetStringDescriptorAscii(uint8 index) {
  if (!VerifyOpen())
    return string();

  // libusb_get_string_descriptor_ascii uses an internal buffer that can only
  // hold up to 128 ASCII characters.
  int length = 128;
  scoped_ptr<uint8[]> data(new uint8[length]);
  int result = libusb_get_string_descriptor_ascii(
      device_handle_, index, data.get(), length);
  if (result < 0) {
    error_.SetFromLibUsbError(static_cast<libusb_error>(result));
    return string();
  }

  error_.Clear();
  return string(reinterpret_cast<const char*>(data.get()), result);
}

}  // namespace mist
