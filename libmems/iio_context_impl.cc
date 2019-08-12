// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include <base/strings/stringprintf.h>
#include <base/logging.h>

#include "libmems/common_types.h"
#include "libmems/iio_channel_impl.h"
#include "libmems/iio_context_impl.h"
#include "libmems/iio_device_impl.h"
#include "libmems/iio_device_trigger_impl.h"

namespace libmems {

IioContextImpl::IioContextImpl() {
  Reload();
}

void IioContextImpl::Reload() {
  // This context will only be destroyed when the entire IioContextImpl goes
  // out of scope. In practice, there will only be at most two contexts
  // in existence (i.e. the initial one and the one we create if we need
  // to initialize the IIO sysfs trigger). This is done in the interest of
  // not having to invalidate existing iio_device pointers, as their lifetime
  // is statically bound to the context that created them (and contexts are
  // themselves static objects that do not update as devices are added
  // and/or removed at runtime).
  context_.push_back({iio_create_local_context(), iio_context_destroy});
  CHECK(GetCurrentContext());
}

iio_context* IioContextImpl::GetCurrentContext() const {
  if (context_.empty())
    return nullptr;
  return context_.back().get();
}

bool IioContextImpl::SetTimeout(uint32_t timeout) {
  int error = iio_context_set_timeout(GetCurrentContext(), timeout);
  if (error) {
    char errMsg[kErrorBufferSize];
    iio_strerror(-error, errMsg, sizeof(errMsg));
    LOG(ERROR) << "Unable to set timeout " << timeout << ": " << errMsg;

    return false;
  }

  return true;
}

std::vector<IioDevice*> IioContextImpl::GetDevicesByName(
    const std::string& name) {
  return GetByName(name, &devices_);
}

IioDevice* IioContextImpl::GetDeviceById(int id) {
  return GetById(id, &devices_);
}

std::vector<IioDevice*> IioContextImpl::GetAllDevices() {
  return GetAll(&devices_);
}

std::vector<IioDevice*> IioContextImpl::GetTriggersByName(
    const std::string& name) {
  return GetByName(name, &triggers_);
}

IioDevice* IioContextImpl::GetTriggerById(int id) {
  return GetById(id, &triggers_);
}

std::vector<IioDevice*> IioContextImpl::GetAllTriggers() {
  return GetAll(&triggers_);
}

template <typename T>
IioDevice* IioContextImpl::GetById(
    int id, std::map<int, std::unique_ptr<T>>* devices_map) {
  auto it_dev = devices_map->find(id);
  if (it_dev != devices_map->end())
    return it_dev->second.get();

  std::string id_str = T::GetStringFromId(id);

  iio_device* device =
      iio_context_find_device(GetCurrentContext(), id_str.c_str());
  if (!device)
    return nullptr;

  devices_map->emplace(id, std::make_unique<T>(this, device));

  return devices_map->at(id).get();
}

template <typename T>
std::vector<IioDevice*> IioContextImpl::GetByName(
    const std::string& name, std::map<int, std::unique_ptr<T>>* devices_map) {
  std::vector<IioDevice*> devices;
  iio_context* ctx = GetCurrentContext();
  uint32_t dev_count = iio_context_get_devices_count(ctx);

  for (uint32_t i = 0; i < dev_count; ++i) {
    iio_device* dev = iio_context_get_device(ctx, i);
    if (!dev) {
      LOG(WARNING) << "Unable to get " << i << "th device";
      continue;
    }

    const char* id_str = iio_device_get_id(dev);
    if (!id_str)
      continue;

    auto id = T::GetIdFromString(id_str);
    if (!id.has_value())
      continue;

    const char* dev_name = iio_device_get_name(dev);
    if (dev_name && name.compare(dev_name) == 0)
      devices.push_back(GetById(id.value(), devices_map));
  }

  return devices;
}

template <typename T>
std::vector<IioDevice*> IioContextImpl::GetAll(
    std::map<int, std::unique_ptr<T>>* devices_map) {
  std::vector<IioDevice*> devices;
  iio_context* ctx = GetCurrentContext();
  uint32_t dev_count = iio_context_get_devices_count(ctx);

  for (uint32_t i = 0; i < dev_count; ++i) {
    iio_device* dev = iio_context_get_device(ctx, i);
    if (!dev) {
      LOG(WARNING) << "Unable to get " << i << "th device";
      continue;
    }

    const char* id_str = iio_device_get_id(dev);
    if (!id_str)
      continue;

    auto id = T::GetIdFromString(id_str);
    if (!id.has_value())
      continue;

    devices.push_back(GetById(id.value(), devices_map));
  }

  return devices;
}

}  // namespace libmems
