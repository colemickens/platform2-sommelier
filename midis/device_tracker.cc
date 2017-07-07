// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "midis/device_tracker.h"

#include <fcntl.h>
#include <sys/ioctl.h>

#include <utility>

#include <base/bind.h>
#include <base/files/file_enumerator.h>
#include <base/files/file_util.h>
#include <base/location.h>
#include <base/memory/ptr_util.h>
#include <base/posix/safe_strerror.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>
#include <base/threading/thread_task_runner_handle.h>
#include <brillo/message_loops/message_loop.h>

namespace {

// udev constants.
const char kUdev[] = "udev";
const char kUdevSubsystemSound[] = "sound";
const char kUdevPropertySoundInitialized[] = "SOUND_INITIALIZED";
const char kUdevActionChange[] = "change";
const char kUdevActionRemove[] = "remove";
const char kUdevIdVendor[] = "ID_VENDOR";
const char kUdevIdVendorId[] = "ID_VENDOR_ID";
const char kUdevIdVendorFromDatabase[] = "ID_VENDOR_FROM_DATABASE";
const char kSysattrVendor[] = "vendor";
const char kSysattrVendorName[] = "vendor_name";

const char kMidiPrefix[] = "midi";

const int kIoctlMaxRetries = 10;

std::string StringOrEmptyIfNull(const char* value) {
  return value ? value : std::string();
}

}  // namespace

namespace midis {

DeviceTracker::DeviceTracker() : udev_handler_(new UdevHandler(this)) {}

UdevHandler::UdevHandler(DeviceTracker* ptr)
    : dev_tracker_(ptr), weak_factory_(this) {}

bool UdevHandler::InitUdevHandler() {
  // Initialize UDEV monitoring.
  udev_.reset(udev_new());
  udev_monitor_.reset(udev_monitor_new_from_netlink(udev_.get(), kUdev));
  if (!udev_monitor_) {
    LOG(ERROR) << "udev_monitor_new_from_netlink fails.";
    return false;
  }

  int err = udev_monitor_filter_add_match_subsystem_devtype(
      udev_monitor_.get(), kUdevSubsystemSound, nullptr);
  if (err != 0) {
    LOG(ERROR) << "udev_monitor_add_match_subsystem fails: "
               << base::safe_strerror(-err);
    return false;
  }

  err = udev_monitor_enable_receiving(udev_monitor_.get());
  if (err != 0) {
    LOG(ERROR) << "udev_monitor_enable_receiving fails: "
               << base::safe_strerror(-err);
    return false;
  }

  udev_monitor_fd_ = base::ScopedFD(udev_monitor_get_fd(udev_monitor_.get()));

  brillo::MessageLoop::current()->WatchFileDescriptor(
      FROM_HERE, udev_monitor_fd_.get(), brillo::MessageLoop::kWatchRead, true,
      base::Bind(&UdevHandler::ProcessUdevFd, weak_factory_.GetWeakPtr(),
                 udev_monitor_fd_.get()));
  return true;
}

bool DeviceTracker::InitDeviceTracker() {
  if (!udev_handler_->InitUdevHandler()) {
    LOG(ERROR) << "Failed to init UdevHandler.";
    return false;
  }

  return true;
}

void UdevHandler::ProcessUdevFd(int fd) {
  std::unique_ptr<udev_device, UdevHandler::UdevDeviceDeleter> dev(
      udev_monitor_receive_device(udev_monitor_.get()));
  if (dev) {
    ProcessUdevEvent(dev.get());
  }
}

std::string UdevHandler::GetMidiDeviceDname(struct udev_device* udev_device) {
  std::string result;

  const char* syspath = udev_device_get_syspath(udev_device);
  if (syspath == nullptr) {
    LOG(ERROR) << "udev_device_get_syspath failed.";
    return result;
  }

  base::FileEnumerator enume(base::FilePath(syspath), false,
                             base::FileEnumerator::DIRECTORIES);
  for (base::FilePath name = enume.Next(); !name.empty(); name = enume.Next()) {
    const std::string cur_name = name.BaseName().value();
    if (base::StartsWith(cur_name, kMidiPrefix, base::CompareCase::SENSITIVE)) {
      result = cur_name;
      LOG(INFO) << "Located MIDI Device: " << result;
      break;
    }
  }

  return result;
}

std::unique_ptr<struct snd_rawmidi_info> UdevHandler::GetDeviceInfo(
    const std::string& dname) {
  uint32_t card, device_num;
  int ret;

  ret = sscanf(dname.c_str(), "midiC%uD%u", &card, &device_num);
  if (ret != 2) {
    LOG(ERROR) << "Couldn't parse card,device number for entry: " << dname;
    return nullptr;
  }

  base::FilePath dev_path("/dev/snd/controlC");
  dev_path = dev_path.InsertBeforeExtension(std::to_string(card));

  base::ScopedFD fd;
  for (int retry_counter = 0; retry_counter < kIoctlMaxRetries;
       ++retry_counter) {
    fd = base::ScopedFD(open(dev_path.value().c_str(), O_RDWR | O_CLOEXEC));
    if (fd.is_valid()) break;

    base::PlatformThread::Sleep(
        base::TimeDelta::FromMilliseconds(2 * (retry_counter + 1)));
  }
  if (!fd.is_valid()) {
    PLOG(ERROR) << "Not able to open device for ioctl: " << dev_path.value();
    return nullptr;
  }

  auto info = base::MakeUnique<snd_rawmidi_info>();
  memset(info.get(), 0, sizeof(snd_rawmidi_info));
  info->device = device_num;
  ret = ioctl(fd.get(), SNDRV_CTL_IOCTL_RAWMIDI_INFO, info.get());

  if (ret < 0) {
    PLOG(ERROR) << "IOCTL SNDRV_CTL_IOCTL_RAWMIDI_INFO failed: "
                << dev_path.value();
    return nullptr;
  }
  return info;
}

dev_t UdevHandler::GetDeviceDevNum(struct udev_device* udev_device) {
  return udev_device_get_devnum(udev_device);
}

const char* UdevHandler::GetDeviceSysNum(struct udev_device* udev_device) {
  return udev_device_get_sysnum(udev_device);
}

// Adapted from midi_manager_alsa.cc in the Chromium source code.
// We use the following preference order to determine the manufacturer:
//  1. Vendor name from h/w device string, from udev props or sysattrs.
//  2. Vendor name from udev database, if it exists.
//  3. The raw device name itself, if all else fails.
std::string UdevHandler::ExtractManufacturerString(
    struct udev_device* udev_device, const std::string& name) {
  std::string vendor = UdevDeviceGetPropertyOrSysAttr(
      udev_device, kUdevIdVendor, kSysattrVendorName);
  std::string vendor_id = UdevDeviceGetPropertyOrSysAttr(
      udev_device, kUdevIdVendorId, kSysattrVendor);

  if (!vendor.empty() && (vendor != vendor_id)) {
    return vendor;
  }

  std::string vendor_id_from_database = StringOrEmptyIfNull(
      udev_device_get_property_value(udev_device, kUdevIdVendorFromDatabase));
  if (!vendor_id_from_database.empty()) {
    return vendor_id_from_database;
  }

  return name;
}

// Directly adopted from Chrome's midi_manager_alsa.cc
const std::string UdevHandler::UdevDeviceGetPropertyOrSysAttr(
    struct udev_device* udev_device, const char* property_key,
    const char* sysattr_key) {
  // First try the property.
  std::string value = StringOrEmptyIfNull(
      udev_device_get_property_value(udev_device, property_key));

  // If no property, look for sysattrs and walk up the parent devices too.
  while (value.empty() && udev_device) {
    value = StringOrEmptyIfNull(
        udev_device_get_sysattr_value(udev_device, sysattr_key));
    udev_device = udev_device_get_parent(udev_device);
  }
  return value;
}

std::unique_ptr<Device> UdevHandler::CreateDevice(
    struct udev_device* udev_device) {
  std::string dname = GetMidiDeviceDname(udev_device);

  if (dname.empty()) {
    LOG(INFO) << "Device connected wasn't a MIDI device.";
    return nullptr;
  }

  auto info = GetDeviceInfo(dname);
  if (!info) {
    LOG(ERROR) << "Couldn't parse info for device: " << dname;
    return nullptr;
  }

  std::string dev_name(reinterpret_cast<char*>(info->name));
  std::string manufacturer(ExtractManufacturerString(udev_device, dev_name));

  return base::MakeUnique<Device>(dev_name, manufacturer, info->card,
                                  info->device, info->subdevices_count,
                                  info->flags);
}

void DeviceTracker::AddDevice(std::unique_ptr<Device> dev) {
  // Get info of new Device
  struct MidisDeviceInfo new_dev;
  FillMidisDeviceInfo(dev.get(), &new_dev);

  uint32_t device_id =
      udev_handler_->GenerateDeviceId(dev->GetCard(), dev->GetDeviceNum());
  devices_.emplace(device_id, std::move(dev));
  NotifyObserversDeviceAddedOrRemoved(&new_dev, true);
}

void DeviceTracker::RemoveDevice(uint32_t sys_num, uint32_t dev_num) {
  auto it = devices_.find(udev_handler_->GenerateDeviceId(sys_num, dev_num));
  if (it != devices_.end()) {
    struct MidisDeviceInfo removed_dev;
    FillMidisDeviceInfo(it->second.get(), &removed_dev);
    devices_.erase(it);
    NotifyObserversDeviceAddedOrRemoved(&removed_dev, false);
    LOG(INFO) << "Device: " << sys_num << "," << dev_num << " removed.";
  } else {
    LOG(ERROR) << "Device: " << sys_num << "," << dev_num << " not listed.";
  }
}

void UdevHandler::ProcessUdevEvent(struct udev_device* udev_device) {
  // We're only interested in card devices, and that too those that are
  // initialized.
  if (!udev_device_get_property_value(udev_device,
                                      kUdevPropertySoundInitialized)) {
    return;
  }

  // Get the action. If no action, then we are doing first time enumeration
  // and the device is treated as new.
  const char* action = udev_device_get_action(udev_device);
  if (!action) {
    action = kUdevActionChange;
  }

  uint32_t sys_num;
  if (!base::StringToUint(GetDeviceSysNum(udev_device), &sys_num)) {
    LOG(ERROR) << "Error retrieving sysnum of device.";
    return;
  }

  uint32_t dev_num = static_cast<uint32_t>(GetDeviceDevNum(udev_device));
  if (strncmp(action, kUdevActionChange, sizeof(kUdevActionChange) - 1) == 0) {
    std::unique_ptr<Device> new_dev = CreateDevice(udev_device);
    if (new_dev) {
      dev_tracker_->AddDevice(std::move(new_dev));
    }
  } else if (strncmp(action, kUdevActionRemove,
                     sizeof(kUdevActionRemove) - 1) == 0) {
    dev_tracker_->RemoveDevice(sys_num, dev_num);
  } else {
    LOG(ERROR) << "Unknown action: " << action;
  }
}

void DeviceTracker::ListDevices(std::vector<MidisDeviceInfo>* list) {
  for (auto& id_device_pair : devices_) {
    list->emplace_back();
    FillMidisDeviceInfo(id_device_pair.second.get(), &list->back());
  }
}

void DeviceTracker::FillMidisDeviceInfo(const Device* dev,
                                        struct MidisDeviceInfo* dev_info) {
  memset(dev_info, 0, sizeof(struct MidisDeviceInfo));
  strncpy(reinterpret_cast<char*>(dev_info->name), dev->GetName().c_str(),
          kMidisStringSize);
  strncpy(reinterpret_cast<char*>(dev_info->manufacturer),
          dev->GetManufacturer().c_str(), kMidisStringSize);
  dev_info->card = dev->GetCard();
  dev_info->device_num = dev->GetDeviceNum();
  dev_info->num_subdevices = dev->GetNumSubdevices();
  dev_info->flags = dev->GetFlags();
}

void DeviceTracker::AddDeviceObserver(Observer* obs) {
  observer_list_.AddObserver(obs);
}

void DeviceTracker::RemoveDeviceObserver(Observer* obs) {
  observer_list_.RemoveObserver(obs);
}

void DeviceTracker::NotifyObserversDeviceAddedOrRemoved(
    struct MidisDeviceInfo* dev_info, bool added) {
  FOR_EACH_OBSERVER(Observer, observer_list_,
                    OnDeviceAddedOrRemoved(dev_info, added));
}

base::ScopedFD DeviceTracker::AddClientToReadSubdevice(uint32_t sys_num,
                                                       uint32_t device_num,
                                                       uint32_t subdevice_num,
                                                       uint32_t client_id) {
  auto it = devices_.find(udev_handler_->GenerateDeviceId(sys_num, device_num));
  if (it != devices_.end()) {
    return it->second->AddClientToReadSubdevice(client_id, subdevice_num);
  }
  return base::ScopedFD();
}

void DeviceTracker::RemoveClientFromDevices(uint32_t client_id) {
  for (auto& id_device_pair : devices_) {
    id_device_pair.second->RemoveClientFromDevice(client_id);
  }
}

}  // namespace midis
