// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mist/usb_device_event_notifier.h"

#include <limits>

#include <base/logging.h>
#include <base/string_number_conversions.h>
#include <base/stringprintf.h>

#include "mist/event_dispatcher.h"
#include "mist/udev.h"
#include "mist/udev_device.h"
#include "mist/udev_monitor.h"
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
      udev_monitor_file_descriptor_(UdevMonitor::kInvalidFileDescriptor) {
  CHECK(dispatcher_);
}

UsbDeviceEventNotifier::~UsbDeviceEventNotifier() {
  if (udev_monitor_file_descriptor_ != UdevMonitor::kInvalidFileDescriptor) {
    dispatcher_->StopWatchingFileDescriptor(udev_monitor_file_descriptor_);
    udev_monitor_file_descriptor_ = UdevMonitor::kInvalidFileDescriptor;
  }
}

bool UsbDeviceEventNotifier::Initialize() {
  udev_.reset(new Udev());
  CHECK(udev_);
  if (!udev_->Initialize()) {
    LOG(ERROR) << "Could not create udev library context.";
    return false;
  }

  udev_monitor_.reset(udev_->CreateMonitorFromNetlink("udev"));
  if (!udev_monitor_) {
    LOG(ERROR) << "Could not create udev monitor.";
    return false;
  }

  if (!udev_monitor_->FilterAddMatchSubsystemDeviceType("usb", "usb_device")) {
    LOG(ERROR) << "Could not add udev monitor filter.";
    return false;
  }

  if (!udev_monitor_->EnableReceiving()) {
    LOG(ERROR) << "Could not enable udev monitoring.";
    return false;
  }

  udev_monitor_file_descriptor_ = udev_monitor_->GetFileDescriptor();
  if (udev_monitor_file_descriptor_ == UdevMonitor::kInvalidFileDescriptor) {
    LOG(ERROR) << "Could not get udev monitor file descriptor.";
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

  scoped_ptr<UdevDevice> device(udev_monitor_->ReceiveDevice());
  if (!device) {
    LOG(WARNING) << "Ignore device event with no associated udev device.";
    return;
  }

  VLOG(1) << StringPrintf("udev (SysPath=%s, Node=%s, Subsystem=%s, "
                          "DevType=%s, Action=%s, VendorId=%s, ProductId=%s)",
                          device->GetSysPath(),
                          device->GetDeviceNode(),
                          device->GetSubsystem(),
                          device->GetDeviceType(),
                          device->GetAction(),
                          device->GetSysAttributeValue(kAttributeIdVendor),
                          device->GetSysAttributeValue(kAttributeIdProduct));

  string sys_path = ConvertNullToEmptyString(device->GetSysPath());
  if (sys_path.empty()) {
    LOG(WARNING) << "Ignore device event with no device sysfs path.";
    return;
  }

  string action = ConvertNullToEmptyString(device->GetAction());
  if (action == "add") {
    string vendor_id_string = ConvertNullToEmptyString(
        device->GetSysAttributeValue(kAttributeIdVendor));
    uint16 vendor_id = 0;
    if (!ConvertIdStringToValue(vendor_id_string, &vendor_id)) {
      LOG(WARNING) << StringPrintf("Invalid USB vendor ID '%s'.",
                                   vendor_id_string.c_str());
      return;
    }

    string product_id_string = ConvertNullToEmptyString(
        device->GetSysAttributeValue(kAttributeIdProduct));
    uint16 product_id = 0;
    if (!ConvertIdStringToValue(product_id_string, &product_id)) {
      LOG(WARNING) << StringPrintf("Invalid USB product ID '%s'.",
                                   product_id_string.c_str());
      return;
    }

    FOR_EACH_OBSERVER(UsbDeviceEventObserver, observer_list_,
                      OnUsbDeviceAdded(sys_path, vendor_id, product_id));
    return;
  }

  if (action == "remove") {
    FOR_EACH_OBSERVER(UsbDeviceEventObserver, observer_list_,
                      OnUsbDeviceRemoved(sys_path));
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
