// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mtpd/device_manager.h"

#include <libudev.h>

#include <set>

#include <base/logging.h>
#include <base/stringprintf.h>
#include <chromeos/callback.h>

#include "mtpd/device_event_delegate.h"

namespace {

const char kPtpInterfaceSignature[] = "6/1/1";
const char kUsbPrefix[] = "usb";
const char kUDevEventType[] = "udev";
const char kUDevUsbSubsystem[] = "usb";

gboolean GlibRunClosure(gpointer data) {
  chromeos::Closure* callback = reinterpret_cast<chromeos::Closure*>(data);
  callback->Run();
  return FALSE;
}

std::string RawDeviceToString(const LIBMTP_raw_device_t& device) {
  return base::StringPrintf("%s:%u,%d", kUsbPrefix, device.bus_location,
                            device.devnum);
}

std::string StorageToString(const std::string& usb_bus_str,
                            uint32_t storage_id) {
  return base::StringPrintf("%s:%u", usb_bus_str.c_str(), storage_id);
}

}  // namespace

namespace mtpd {

DeviceManager::DeviceManager(DeviceEventDelegate* delegate)
    : udev_(udev_new()),
      udev_monitor_(NULL),
      udev_monitor_fd_(-1),
      delegate_(delegate) {
  // Set up udev monitoring.
  CHECK(delegate_);
  CHECK(udev_);
  udev_monitor_ = udev_monitor_new_from_netlink(udev_, kUDevEventType);
  CHECK(udev_monitor_);
  int ret = udev_monitor_filter_add_match_subsystem_devtype(udev_monitor_,
                                                            kUDevUsbSubsystem,
                                                            NULL);
  CHECK_EQ(0, ret);
  ret = udev_monitor_enable_receiving(udev_monitor_);
  CHECK_EQ(0, ret);
  udev_monitor_fd_ = udev_monitor_get_fd(udev_monitor_);
  CHECK_GE(udev_monitor_fd_, 0);

  // Initialize libmtp.
  LIBMTP_Init();

  // Trigger a device scan.
  AddDevices(NULL /* no callback source */);
}

DeviceManager::~DeviceManager() {
  RemoveDevices(true /* remove all */);
}

int DeviceManager::GetDeviceEventDescriptor() const {
  return udev_monitor_fd_;
}

void DeviceManager::ProcessDeviceEvents() {
  udev_device* dev = udev_monitor_receive_device(udev_monitor_);
  CHECK(dev);
  HandleDeviceNotification(dev);
  udev_device_unref(dev);
}

std::vector<std::string> DeviceManager::EnumerateStorage() {
  std::vector<std::string> ret;
  for (MtpDeviceMap::const_iterator device_it = device_map_.begin();
       device_it != device_map_.end();
       ++device_it) {
    const std::string& usb_bus_str = device_it->first;
    const MtpStorageMap& storage_map = device_it->second.second;
    for (MtpStorageMap::const_iterator storage_it = storage_map.begin();
         storage_it != storage_map.end();
         ++storage_it) {
      ret.push_back(StorageToString(usb_bus_str, storage_it->first));
      LOG(INFO) << "EnumerateStorage "
                << StorageToString(usb_bus_str, storage_it->first);
    }
  }
  return ret;
}

void DeviceManager::HandleDeviceNotification(udev_device* device) {
  const char* action = udev_device_get_property_value(device, "ACTION");
  const char* interface = udev_device_get_property_value(device, "INTERFACE");
  if (!action || !interface)
    return;

  const std::string kEventAction(action);
  const std::string kEventInterface(interface);
  if (kEventInterface != kPtpInterfaceSignature)
    return;

  if (kEventAction == "add") {
    // Some devices do not respond well when immediately probed. Thus there is
    // a 1 second wait here to give the device to settle down.
    GSource* source = g_timeout_source_new_seconds(1);
    chromeos::Closure* closure =
        chromeos::NewCallback(this, &DeviceManager::AddDevices, source);
    g_source_set_callback(source, &GlibRunClosure, closure, NULL);
    g_source_attach(source, NULL);
    return;
  }
  if (kEventAction == "remove") {
    RemoveDevices(false /* !remove_all */);
    return;
  }
  NOTREACHED();
}

void DeviceManager::AddDevices(GSource* source) {
  if (source) {
    // Matches g_source_attach().
    g_source_destroy(source);
    // Matches the implicit add-ref in g_timeout_source_new_seconds().
    g_source_unref(source);
  }

  // Get raw devices.
  LIBMTP_raw_device_t* raw_devices = NULL;
  int raw_devices_count = 0;
  LIBMTP_error_number_t err =
      LIBMTP_Detect_Raw_Devices(&raw_devices, &raw_devices_count);
  if (err != LIBMTP_ERROR_NONE) {
    LOG(ERROR) << "LIBMTP_Detect_Raw_Devices failed with " << err;
    return;
  }

  // Iterate through raw devices.
  for (int i = 0; i < raw_devices_count; ++i) {
    // Open the mtp device.
    const std::string usb_bus_str = RawDeviceToString(raw_devices[i]);
    LIBMTP_mtpdevice_t* mtp_device =
        LIBMTP_Open_Raw_Device_Uncached(&raw_devices[i]);
    if (!mtp_device) {
      LOG(ERROR) << "LIBMTP_Open_Raw_Device_Uncached failed for "
                 << usb_bus_str;
      continue;
    }

    // Iterate through storages on the device and add them.
    MtpStorageMap storage_map;
    for (LIBMTP_devicestorage_t* storage = mtp_device->storage;
         storage != NULL;
         storage = storage->next) {
      StorageInfo info(raw_devices[i].device_entry, *storage);
      storage_map.insert(std::make_pair(storage->id, info));
      delegate_->StorageAttached(StorageToString(usb_bus_str, storage->id));
      LOG(INFO) << "Added storage " << storage->id
                << " on device " << usb_bus_str;
    }
    device_map_.insert(
        std::make_pair(usb_bus_str, std::make_pair(mtp_device, storage_map)));
    LOG(INFO) << "Added device " << usb_bus_str << " with "
              << storage_map.size() << " storages";
  }
  free(raw_devices);
}

void DeviceManager::RemoveDevices(bool remove_all) {
  LIBMTP_raw_device_t* raw_devices = NULL;
  int raw_devices_count = 0;

  if (!remove_all) {
    LIBMTP_error_number_t err =
        LIBMTP_Detect_Raw_Devices(&raw_devices, &raw_devices_count);
    if (!(err == LIBMTP_ERROR_NONE || err == LIBMTP_ERROR_NO_DEVICE_ATTACHED)) {
      LOG(ERROR) << "LIBMTP_Detect_Raw_Devices failed with " << err;
      return;
    }
  }

  // Populate |devices_set| with all known attached devices.
  typedef std::set<std::string> MtpDeviceSet;
  MtpDeviceSet devices_set;
  for (MtpDeviceMap::const_iterator it = device_map_.begin();
       it != device_map_.end();
       ++it) {
    devices_set.insert(it->first);
  }

  // And remove the ones that are still attached.
  for (int i = 0; i < raw_devices_count; ++i)
    devices_set.erase(RawDeviceToString(raw_devices[i]));

  // The ones left in the set are the detached devices.
  for (MtpDeviceSet::const_iterator it = devices_set.begin();
       it != devices_set.end();
       ++it) {
    LOG(INFO) << "Removed " << *it;
    MtpDeviceMap::iterator device_it = device_map_.find(*it);
    if (device_it == device_map_.end()) {
      NOTREACHED();
      continue;
    }

    // Remove all the storages on that device.
    const std::string& usb_bus_str = device_it->first;
    const MtpStorageMap& storage_map = device_it->second.second;
    for (MtpStorageMap::const_iterator storage_it = storage_map.begin();
         storage_it != storage_map.end();
         ++storage_it) {
      delegate_->StorageDetached(
          StorageToString(usb_bus_str, storage_it->first));
    }

    // Delete the device's map entry and cleanup.
    LIBMTP_mtpdevice_t* mtp_device = device_it->second.first;
    device_map_.erase(device_it);

    // When |remove_all| is false, the device has already been detached
    // and this runs after the fact. As such, this call will very
    // likely fail and spew a bunch of error messages. Call it anyway to
    // let libmtp do any cleanup it can.
    LIBMTP_Release_Device(mtp_device);
  }
}

}  // namespace mtpd
