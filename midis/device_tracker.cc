// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "midis/device_tracker.h"

#include <fcntl.h>

#include <utility>

#include <base/bind.h>
#include <base/location.h>
#include <base/memory/ptr_util.h>

#include "midis/seq_handler.h"

namespace midis {

DeviceTracker::DeviceTracker() {}

bool DeviceTracker::InitDeviceTracker() {
  seq_handler_ = base::MakeUnique<SeqHandler>(
      base::Bind(&DeviceTracker::AddDevice, base::Unretained(this)),
      base::Bind(&DeviceTracker::RemoveDevice, base::Unretained(this)),
      base::Bind(&DeviceTracker::HandleReceiveData, base::Unretained(this)),
      base::Bind(&DeviceTracker::IsDevicePresent, base::Unretained(this)),
      base::Bind(&DeviceTracker::IsPortPresent, base::Unretained(this)));

  if (!seq_handler_->InitSeq()) {
    LOG(ERROR) << "Failed to start snd_seq tracker.";
    return false;
  }

  return true;
}

void DeviceTracker::AddDevice(std::unique_ptr<Device> dev) {
  // Get info of new Device
  struct MidisDeviceInfo new_dev;
  FillMidisDeviceInfo(dev.get(), &new_dev);

  uint32_t device_id = GenerateDeviceId(dev->GetCard(), dev->GetDeviceNum());
  devices_.emplace(device_id, std::move(dev));
  NotifyObserversDeviceAddedOrRemoved(&new_dev, true);
}

void DeviceTracker::RemoveDevice(uint32_t sys_num, uint32_t dev_num) {
  auto it = devices_.find(GenerateDeviceId(sys_num, dev_num));
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

void DeviceTracker::ListDevices(std::vector<MidisDeviceInfo>* list) {
  for (auto& id_device_pair : devices_) {
    list->emplace_back();
    FillMidisDeviceInfo(id_device_pair.second.get(), &list->back());
  }
}

void DeviceTracker::FillMidisDeviceInfo(const Device* dev,
                                        struct MidisDeviceInfo* dev_info) {
  memset(dev_info, 0, sizeof(struct MidisDeviceInfo));
  strncpy(reinterpret_cast<char*>(dev_info->name),
          dev->GetName().c_str(),
          kMidisStringSize);
  strncpy(reinterpret_cast<char*>(dev_info->manufacturer),
          dev->GetManufacturer().c_str(),
          kMidisStringSize);
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
  FOR_EACH_OBSERVER(
      Observer, observer_list_, OnDeviceAddedOrRemoved(dev_info, added));
}

base::ScopedFD DeviceTracker::AddClientToReadSubdevice(uint32_t sys_num,
                                                       uint32_t device_num,
                                                       uint32_t subdevice_num,
                                                       uint32_t client_id) {
  Device* device = FindDevice(sys_num, device_num);
  if (device) {
    return device->AddClientToReadSubdevice(client_id, subdevice_num);
  }
  return base::ScopedFD();
}

void DeviceTracker::RemoveClientFromDevice(uint32_t client_id,
                                           uint32_t sys_num,
                                           uint32_t device_num) {
  Device* device = FindDevice(sys_num, device_num);
  if (device) {
    device->RemoveClientFromDevice(client_id);
  }
}

void DeviceTracker::RemoveClientFromDevices(uint32_t client_id) {
  for (auto& id_device_pair : devices_) {
    id_device_pair.second->RemoveClientFromDevice(client_id);
  }
}

void DeviceTracker::HandleReceiveData(uint32_t card_id,
                                      uint32_t device_id,
                                      uint32_t port_id,
                                      const char* buffer,
                                      size_t buf_len) {
  Device* device = FindDevice(card_id, device_id);
  if (device) {
    device->HandleReceiveData(buffer, port_id, buf_len);
  }
}

bool DeviceTracker::IsDevicePresent(uint32_t card_id, uint32_t device_id) {
  Device* device = FindDevice(card_id, device_id);
  if (device) {
    return true;
  }
  return false;
}

bool DeviceTracker::IsPortPresent(uint32_t card_id,
                                  uint32_t device_id,
                                  uint32_t port_id) {
  Device* device = FindDevice(card_id, device_id);
  if (!device) {
    return false;
  }

  // TODO(pmalani): Check |port_caps_| instead.
  if (port_id >= device->GetNumSubdevices()) {
    return false;
  }
  return true;
}

Device* DeviceTracker::FindDevice(uint32_t card_id, uint32_t device_id) const {
  auto it = devices_.find(GenerateDeviceId(card_id, device_id));
  if (it != devices_.end()) {
    return it->second.get();
  }
  return nullptr;
}

}  // namespace midis
