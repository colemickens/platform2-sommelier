// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mist/usb_manager.h"

#include <libusb.h>

#include <base/logging.h>

#include "mist/usb_device.h"

namespace mist {

UsbManager::UsbManager() : context_(NULL) {}

UsbManager::~UsbManager() {
  if (context_) {
    libusb_exit(context_);
    context_ = NULL;
  }
}

bool UsbManager::Initialize() {
  CHECK(!context_);

  int result = libusb_init(&context_);
  if (!error_.SetFromLibUsbError(static_cast<libusb_error>(result))) {
    LOG(ERROR) << "Could not initialize libusb: " << error_;
    return false;
  }
  return true;
}

void UsbManager::SetDebugLevel(int level) {
  CHECK(context_);

  libusb_set_debug(context_, level);
}

bool UsbManager::GetDevices(ScopedVector<UsbDevice>* devices) {
  CHECK(context_);
  CHECK(devices);

  devices->clear();

  libusb_device** device_list = NULL;
  ssize_t result = libusb_get_device_list(context_, &device_list);
  if (result < 0)
    return error_.SetFromLibUsbError(static_cast<libusb_error>(result));

  for (ssize_t i = 0; i < result; ++i) {
    devices->push_back(new UsbDevice(device_list[i]));
  }

  // UsbDevice holds a reference count of a libusb_device struct. Thus,
  // decrement the reference count of the libusb_device struct in the list by
  // one when freeing the list.
  libusb_free_device_list(device_list, 1);
  return true;
}

}  // namespace mist
