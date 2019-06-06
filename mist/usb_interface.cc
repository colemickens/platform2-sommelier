// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mist/usb_interface.h"

#include <libusb.h>

#include <base/logging.h>
#include <base/strings/stringprintf.h>

#include "mist/usb_device.h"
#include "mist/usb_interface_descriptor.h"

using base::StringPrintf;

namespace mist {

UsbInterface::UsbInterface(const base::WeakPtr<UsbDevice>& device,
                           const libusb_interface* interface)
    : device_(device), interface_(interface) {
  CHECK(interface_);
}

int UsbInterface::GetNumAlternateSettings() const {
  return interface_->num_altsetting;
}

std::unique_ptr<UsbInterfaceDescriptor> UsbInterface::GetAlternateSetting(
    int index) const {
  if (index < 0 || index >= GetNumAlternateSettings()) {
    LOG(ERROR) << StringPrintf(
        "Invalid alternate setting index %d. "
        "Must be non-negative and less than %d.",
        index, GetNumAlternateSettings());
    return nullptr;
  }

  return std::make_unique<UsbInterfaceDescriptor>(
      device_, &interface_->altsetting[index]);
}

}  // namespace mist
