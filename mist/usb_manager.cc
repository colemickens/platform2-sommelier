// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mist/usb_manager.h"

#include <libusb.h>
#include <poll.h>

#include <memory>
#include <utility>

#include <base/logging.h>
#include <base/memory/free_deleter.h>
#include <base/strings/stringprintf.h>

#include "mist/event_dispatcher.h"
#include "mist/usb_device.h"
#include "mist/usb_device_descriptor.h"

using base::MessageLoopForIO;
using base::StringPrintf;
using std::unique_ptr;

namespace mist {

namespace {

MessageLoopForIO::Mode ConvertEventFlagsToWatchMode(short events) {  // NOLINT
  if ((events & POLLIN) && (events & POLLOUT))
    return MessageLoopForIO::WATCH_READ_WRITE;

  if (events & POLLIN)
    return MessageLoopForIO::WATCH_READ;

  if (events & POLLOUT)
    return MessageLoopForIO::WATCH_WRITE;

  return MessageLoopForIO::WATCH_READ_WRITE;
}

}  // namespace

UsbManager::UsbManager(EventDispatcher* dispatcher)
    : dispatcher_(dispatcher), context_(nullptr) {}

UsbManager::~UsbManager() {
  if (context_) {
    libusb_exit(context_);
    context_ = nullptr;
  }
}

bool UsbManager::Initialize() {
  CHECK(!context_);

  int result = libusb_init(&context_);
  if (!error_.SetFromLibUsbError(static_cast<libusb_error>(result))) {
    LOG(ERROR) << "Could not initialize libusb: " << error_;
    return false;
  }

  if (!StartWatchingPollFileDescriptors()) {
    error_.set_type(UsbError::kErrorNotSupported);
    return false;
  }

  return true;
}

void UsbManager::SetDebugLevel(int level) {
  CHECK(context_);

  libusb_set_option(context_, LIBUSB_OPTION_LOG_LEVEL, level);
}

std::unique_ptr<UsbDevice> UsbManager::GetDevice(uint8_t bus_number,
                                                 uint8_t device_address,
                                                 uint16_t vendor_id,
                                                 uint16_t product_id) {
  std::vector<std::unique_ptr<UsbDevice>> devices;
  if (!GetDevices(&devices))
    return nullptr;

  for (auto& device : devices) {
    if (device->GetBusNumber() != bus_number ||
        device->GetDeviceAddress() != device_address)
      continue;

    unique_ptr<UsbDeviceDescriptor> device_descriptor =
        device->GetDeviceDescriptor();
    VLOG(2) << *device_descriptor;
    if (device_descriptor->GetVendorId() == vendor_id &&
        device_descriptor->GetProductId() == product_id) {
      return std::move(device);
    }
  }

  error_.set_type(UsbError::kErrorNotFound);
  return nullptr;
}

bool UsbManager::GetDevices(std::vector<std::unique_ptr<UsbDevice>>* devices) {
  CHECK(context_);
  CHECK(devices);

  devices->clear();

  libusb_device** device_list = nullptr;
  ssize_t result = libusb_get_device_list(context_, &device_list);
  if (result < 0)
    return error_.SetFromLibUsbError(static_cast<libusb_error>(result));

  for (ssize_t i = 0; i < result; ++i) {
    devices->push_back(std::make_unique<UsbDevice>(device_list[i]));
  }

  // UsbDevice holds a reference count of a libusb_device struct. Thus,
  // decrement the reference count of the libusb_device struct in the list by
  // one when freeing the list.
  libusb_free_device_list(device_list, 1);
  return true;
}

void UsbManager::OnPollFileDescriptorAdded(int file_descriptor,
                                           short events,  // NOLINT
                                           void* user_data) {
  CHECK(user_data);

  VLOG(2) << StringPrintf("Poll file descriptor %d on events 0x%016x added.",
                          file_descriptor, events);
  UsbManager* manager = reinterpret_cast<UsbManager*>(user_data);
  manager->dispatcher_->StartWatchingFileDescriptor(
      file_descriptor, ConvertEventFlagsToWatchMode(events), manager);
}

void UsbManager::OnPollFileDescriptorRemoved(int file_descriptor,
                                             void* user_data) {
  CHECK(user_data);

  VLOG(2) << StringPrintf("Poll file descriptor %d removed.", file_descriptor);
  UsbManager* manager = reinterpret_cast<UsbManager*>(user_data);
  manager->dispatcher_->StopWatchingFileDescriptor(file_descriptor);
}

bool UsbManager::StartWatchingPollFileDescriptors() {
  CHECK(context_);

  libusb_set_pollfd_notifiers(context_, &OnPollFileDescriptorAdded,
                              &OnPollFileDescriptorRemoved, this);

  unique_ptr<const libusb_pollfd*, base::FreeDeleter> pollfd_list(
      libusb_get_pollfds(context_));
  if (!pollfd_list) {
    LOG(ERROR) << "Could not get file descriptors for monitoring USB events.";
    return false;
  }

  for (const libusb_pollfd** fd_ptr = pollfd_list.get(); *fd_ptr; ++fd_ptr) {
    const libusb_pollfd& pollfd = *(*fd_ptr);
    VLOG(2) << StringPrintf("Poll file descriptor %d for events 0x%016x added.",
                            pollfd.fd, pollfd.events);
    if (!dispatcher_->StartWatchingFileDescriptor(
            pollfd.fd, ConvertEventFlagsToWatchMode(pollfd.events), this)) {
      return false;
    }
  }
  return true;
}

void UsbManager::HandleEventsNonBlocking() {
  CHECK(context_);

  timeval zero_tv = {0};
  int result =
      libusb_handle_events_timeout_completed(context_, &zero_tv, nullptr);
  UsbError error(static_cast<libusb_error>(result));
  LOG_IF(ERROR, !error.IsSuccess()) << "Could not handle USB events: " << error;
}

void UsbManager::OnFileCanReadWithoutBlocking(int file_descriptor) {
  VLOG(3) << StringPrintf("File descriptor %d available for read.",
                          file_descriptor);
  HandleEventsNonBlocking();
}

void UsbManager::OnFileCanWriteWithoutBlocking(int file_descriptor) {
  VLOG(3) << StringPrintf("File descriptor %d available for write.",
                          file_descriptor);
  HandleEventsNonBlocking();
}

}  // namespace mist
