// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mist/usb_device_event_notifier.h"

#include <string>

#include <libudev.h>

#include <base/logging.h>
#include <base/string_number_conversions.h>
#include <base/stringprintf.h>

#include "mist/event_dispatcher.h"
#include "mist/usb_device_event_observer.h"

using base::StringPrintf;
using std::string;

namespace mist {

namespace {

const char kAttributeIdProduct[] = "idProduct";
const char kAttributeIdVendor[] = "idVendor";

}  // namespace

UsbDeviceEventNotifier::UsbDeviceEventNotifier(EventDispatcher* dispatcher)
    : dispatcher_(dispatcher),
      udev_(NULL),
      udev_monitor_(NULL),
      udev_monitor_file_descriptor_(0) {
  CHECK(dispatcher_);
}

UsbDeviceEventNotifier::~UsbDeviceEventNotifier() {
  if (udev_monitor_file_descriptor_ > 0) {
    dispatcher_->StopWatchingFileDescriptor(udev_monitor_file_descriptor_);
    udev_monitor_file_descriptor_ = 0;
  }

  if (udev_monitor_) {
    udev_monitor_unref(udev_monitor_);
    udev_monitor_ = NULL;
  }

  if (udev_) {
    udev_unref(udev_);
    udev_ = NULL;
  }
}

bool UsbDeviceEventNotifier::Initialize() {
  udev_ = udev_new();
  if (!udev_) {
    LOG(ERROR) << "Could not create udev library context.";
    return false;
  }

  udev_monitor_ = udev_monitor_new_from_netlink(udev_, "udev");
  if (!udev_monitor_) {
    LOG(ERROR) << "Could not create udev monitor.";
    return false;
  }

  int result = udev_monitor_filter_add_match_subsystem_devtype(
      udev_monitor_, "usb", "usb_device");
  if (result != 0) {
    LOG(ERROR) << StringPrintf("Could not add udev monitor filter (error=%d).",
                               result);
    return false;
  }

  result = udev_monitor_enable_receiving(udev_monitor_);
  if (result != 0) {
    LOG(ERROR) << StringPrintf("Could not enable udev monitoring (error=%d).",
                               result);
    return false;
  }

  udev_monitor_file_descriptor_ = udev_monitor_get_fd(udev_monitor_);
  if (udev_monitor_file_descriptor_ <= 0) {
    LOG(ERROR) << StringPrintf("Could not get udev monitor file descriptor.");
    return false;
  }

  if (!dispatcher_->StartWatchingFileDescriptor(
      udev_monitor_file_descriptor_, MessageLoopForIO::WATCH_READ, this)) {
    LOG(ERROR) << "Could not watch udev monitor file descriptor.";
    return false;
  }

  return true;
}

void UsbDeviceEventNotifier::AddObserver(UsbDeviceEventObserver* observer) {
  CHECK(observer);

  observer_list_.AddObserver(observer);
}

void UsbDeviceEventNotifier::RemoveObserver(UsbDeviceEventObserver* observer) {
  CHECK(observer);

  observer_list_.RemoveObserver(observer);
}

void UsbDeviceEventNotifier::OnFileCanReadWithoutBlocking(int file_descriptor) {
  VLOG(3) << StringPrintf("File descriptor %d available for read.",
                          file_descriptor);

  udev_device* device = udev_monitor_receive_device(udev_monitor_);
  if (!device) {
    LOG(WARNING) << "Ignore device event with no associated udev device.";
    return;
  }

  VLOG(1) << StringPrintf("udev (SysPath=%s, Node=%s, Subsystem=%s, "
                          "DevType=%s, Action=%s, VendorId=%s, ProductId=%s)",
                          udev_device_get_syspath(device),
                          udev_device_get_devnode(device),
                          udev_device_get_subsystem(device),
                          udev_device_get_devtype(device),
                          udev_device_get_action(device),
                          udev_device_get_sysattr_value(device,
                                                        kAttributeIdVendor),
                          udev_device_get_sysattr_value(device,
                                                        kAttributeIdProduct));

  string syspath = ConvertNullToEmptyString(udev_device_get_syspath(device));
  string action = ConvertNullToEmptyString(udev_device_get_action(device));
  udev_device_unref(device);

  if (syspath.empty()) {
    LOG(WARNING) << "Ignore device event with no device sysfs path.";
    return;
  }

  if (action == "add") {
    string vendor_id_string = ConvertNullToEmptyString(
        udev_device_get_sysattr_value(device, kAttributeIdVendor));
    uint16 vendor_id = 0;
    if (!ConvertIdStringToValue(vendor_id_string, &vendor_id)) {
      LOG(WARNING) << StringPrintf("Invalid USB vendor ID '%s'.",
                                   vendor_id_string.c_str());
      return;
    }

    string product_id_string = ConvertNullToEmptyString(
        udev_device_get_sysattr_value(device, kAttributeIdProduct));
    uint16 product_id = 0;
    if (!ConvertIdStringToValue(product_id_string, &product_id)) {
      LOG(WARNING) << StringPrintf("Invalid USB product ID '%s'.",
                                   product_id_string.c_str());
      return;
    }

    FOR_EACH_OBSERVER(UsbDeviceEventObserver, observer_list_,
                      OnUsbDeviceAdded(syspath, vendor_id, product_id));
    return;
  }

  if (action == "remove") {
    FOR_EACH_OBSERVER(UsbDeviceEventObserver, observer_list_,
                      OnUsbDeviceRemoved(syspath));
  }
}

void UsbDeviceEventNotifier::OnFileCanWriteWithoutBlocking(
    int file_descriptor) {
  NOTREACHED();
}

// static
std::string UsbDeviceEventNotifier::ConvertNullToEmptyString(const char* str) {
  return str ? str : string();
}

// static
bool UsbDeviceEventNotifier::ConvertIdStringToValue(const string& id_string,
                                                    uint16* id) {
  int value = -1;
  if (id_string.size() != 4 ||
      !base::HexStringToInt(id_string, &value) ||
      value < 0 ||
      value > std::numeric_limits<uint16>::max()) {
    return false;
  }

  *id = static_cast<uint16>(value);
  return true;
}

}  // namespace mist
