// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mist/usb_device_event_notifier.h"

#include <limits>

#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/stringprintf.h>
#include <brillo/udev/udev.h>
#include <brillo/udev/udev_device.h>
#include <brillo/udev/udev_enumerate.h>
#include <brillo/udev/udev_monitor.h>

#include "mist/event_dispatcher.h"
#include "mist/usb_device_event_observer.h"

using base::MessageLoopForIO;
using base::StringPrintf;
using std::string;
using std::unique_ptr;

namespace mist {

namespace {

const char kAttributeBusNumber[] = "busnum";
const char kAttributeDeviceAddress[] = "devnum";
const char kAttributeIdProduct[] = "idProduct";
const char kAttributeIdVendor[] = "idVendor";

}  // namespace

UsbDeviceEventNotifier::UsbDeviceEventNotifier(EventDispatcher* dispatcher,
                                               brillo::Udev* udev)
    : dispatcher_(dispatcher),
      udev_(udev),
      udev_monitor_file_descriptor_(
          brillo::UdevMonitor::kInvalidFileDescriptor) {
  CHECK(dispatcher_);
  CHECK(udev_);
}

UsbDeviceEventNotifier::~UsbDeviceEventNotifier() {
  if (udev_monitor_file_descriptor_ !=
      brillo::UdevMonitor::kInvalidFileDescriptor) {
    dispatcher_->StopWatchingFileDescriptor(udev_monitor_file_descriptor_);
    udev_monitor_file_descriptor_ = brillo::UdevMonitor::kInvalidFileDescriptor;
  }
}

bool UsbDeviceEventNotifier::Initialize() {
  udev_monitor_ = udev_->CreateMonitorFromNetlink("udev");
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
  if (udev_monitor_file_descriptor_ ==
      brillo::UdevMonitor::kInvalidFileDescriptor) {
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

bool UsbDeviceEventNotifier::ScanExistingDevices() {
  unique_ptr<brillo::UdevEnumerate> enumerate = udev_->CreateEnumerate();
  if (!enumerate || !enumerate->AddMatchSubsystem("usb") ||
      !enumerate->AddMatchProperty("DEVTYPE", "usb_device") ||
      !enumerate->ScanDevices()) {
    LOG(ERROR) << "Could not enumerate USB devices on the system.";
    return false;
  }

  for (unique_ptr<brillo::UdevListEntry> list_entry = enumerate->GetListEntry();
       list_entry; list_entry = list_entry->GetNext()) {
    string sys_path = ConvertNullToEmptyString(list_entry->GetName());

    unique_ptr<brillo::UdevDevice> device =
        udev_->CreateDeviceFromSysPath(sys_path.c_str());
    if (!device)
      continue;

    uint8_t bus_number;
    uint8_t device_address;
    uint16_t vendor_id;
    uint16_t product_id;
    if (!GetDeviceAttributes(device.get(), &bus_number, &device_address,
                             &vendor_id, &product_id))
      continue;

    for (UsbDeviceEventObserver& observer : observer_list_) {
      observer.OnUsbDeviceAdded(sys_path, bus_number, device_address, vendor_id,
                                product_id);
    }
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

  unique_ptr<brillo::UdevDevice> device = udev_monitor_->ReceiveDevice();
  if (!device) {
    LOG(WARNING) << "Ignore device event with no associated udev device.";
    return;
  }

  VLOG(1) << StringPrintf(
      "udev (SysPath=%s, "
      "Node=%s, "
      "Subsystem=%s, "
      "DevType=%s, "
      "Action=%s, "
      "BusNumber=%s, "
      "DeviceAddress=%s, "
      "VendorId=%s, "
      "ProductId=%s)",
      device->GetSysPath(), device->GetDeviceNode(), device->GetSubsystem(),
      device->GetDeviceType(), device->GetAction(),
      device->GetSysAttributeValue(kAttributeBusNumber),
      device->GetSysAttributeValue(kAttributeDeviceAddress),
      device->GetSysAttributeValue(kAttributeIdVendor),
      device->GetSysAttributeValue(kAttributeIdProduct));

  string sys_path = ConvertNullToEmptyString(device->GetSysPath());
  if (sys_path.empty()) {
    LOG(WARNING) << "Ignore device event with no device sysfs path.";
    return;
  }

  string action = ConvertNullToEmptyString(device->GetAction());
  if (action == "add") {
    uint8_t bus_number;
    uint8_t device_address;
    uint16_t vendor_id;
    uint16_t product_id;
    if (!GetDeviceAttributes(device.get(), &bus_number, &device_address,
                             &vendor_id, &product_id)) {
      LOG(WARNING) << "Ignore device event of unidentifiable device.";
      return;
    }

    for (UsbDeviceEventObserver& observer : observer_list_) {
      observer.OnUsbDeviceAdded(sys_path, bus_number, device_address, vendor_id,
                                product_id);
    }
    return;
  }

  if (action == "remove") {
    for (UsbDeviceEventObserver& observer : observer_list_)
      observer.OnUsbDeviceRemoved(sys_path);
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
bool UsbDeviceEventNotifier::ConvertHexStringToUint16(const string& str,
                                                      uint16_t* value) {
  int temp_value = -1;
  if (str.size() != 4 || !base::HexStringToInt(str, &temp_value) ||
      temp_value < 0 || temp_value > std::numeric_limits<uint16_t>::max()) {
    return false;
  }

  *value = static_cast<uint16_t>(temp_value);
  return true;
}

// static
bool UsbDeviceEventNotifier::ConvertStringToUint8(const string& str,
                                                  uint8_t* value) {
  unsigned temp_value = 0;
  if (!base::StringToUint(str, &temp_value) ||
      temp_value > std::numeric_limits<uint8_t>::max()) {
    return false;
  }

  *value = static_cast<uint8_t>(temp_value);
  return true;
}

// static
bool UsbDeviceEventNotifier::GetDeviceAttributes(
    const brillo::UdevDevice* device,
    uint8_t* bus_number,
    uint8_t* device_address,
    uint16_t* vendor_id,
    uint16_t* product_id) {
  string bus_number_string = ConvertNullToEmptyString(
      device->GetSysAttributeValue(kAttributeBusNumber));
  if (!ConvertStringToUint8(bus_number_string, bus_number)) {
    LOG(WARNING) << "Invalid USB bus number '" << bus_number_string << "'.";
    return false;
  }

  string device_address_string = ConvertNullToEmptyString(
      device->GetSysAttributeValue(kAttributeDeviceAddress));
  if (!ConvertStringToUint8(device_address_string, device_address)) {
    LOG(WARNING) << "Invalid USB device address '" << device_address_string
                 << "'.";
    return false;
  }

  string vendor_id_string = ConvertNullToEmptyString(
      device->GetSysAttributeValue(kAttributeIdVendor));
  if (!ConvertHexStringToUint16(vendor_id_string, vendor_id)) {
    LOG(WARNING) << "Invalid USB vendor ID '" << vendor_id_string << "'.";
    return false;
  }

  string product_id_string = ConvertNullToEmptyString(
      device->GetSysAttributeValue(kAttributeIdProduct));
  if (!ConvertHexStringToUint16(product_id_string, product_id)) {
    LOG(WARNING) << "Invalid USB product ID '" << product_id_string << "'.";
    return false;
  }

  return true;
}

}  // namespace mist
