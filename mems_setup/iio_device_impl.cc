// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include <base/files/file_util.h>
#include <base/logging.h>

#include "mems_setup/iio_channel_impl.h"
#include "mems_setup/iio_context_impl.h"
#include "mems_setup/iio_device_impl.h"

namespace mems_setup {

IioDeviceImpl::IioDeviceImpl(IioContextImpl* ctx, iio_device* dev)
    : IioDevice(), context_(ctx), device_(dev) {
  CHECK(context_);
  CHECK(device_);
}

IioContext* IioDeviceImpl::GetContext() const {
  return context_;
}

const char* IioDeviceImpl::GetName() const {
  return iio_device_get_name(device_);
}

const char* IioDeviceImpl::GetId() const {
  return iio_device_get_id(device_);
}

base::FilePath IioDeviceImpl::GetPath() const {
  auto path = base::FilePath("/sys/bus/iio/devices").Append(GetId());
  CHECK(base::DirectoryExists(path));
  return path;
}

base::Optional<std::string> IioDeviceImpl::ReadStringAttribute(
    const std::string& name) const {
  char data[1024] = {0};
  ssize_t len = iio_device_attr_read(device_, name.c_str(), data, sizeof(data));
  if (len < 0) {
    LOG(WARNING) << "Attempting to read attribute " << name
                 << " failed: " << len;
    return base::nullopt;
  }
  return std::string(data, len);
}

base::Optional<int64_t> IioDeviceImpl::ReadNumberAttribute(
    const std::string& name) const {
  long long val = 0;  // NOLINT(runtime/int)
  int error = iio_device_attr_read_longlong(device_, name.c_str(), &val);
  if (error) {
    LOG(WARNING) << "Attempting to read attribute " << name
                 << " failed: " << error;
    return base::nullopt;
  }
  return val;
}

bool IioDeviceImpl::WriteStringAttribute(const std::string& name,
                                         const std::string& val) {
  int error =
      iio_device_attr_write_raw(device_, name.c_str(), val.data(), val.size());
  if (error) {
    LOG(WARNING) << "Attempting to write attribute " << name
                 << " failed: " << error;
    return false;
  }
  return true;
}
bool IioDeviceImpl::WriteNumberAttribute(const std::string& name,
                                         int64_t val) {
  int error = iio_device_attr_write_longlong(device_, name.c_str(), val);
  if (error) {
    LOG(WARNING) << "Attempting to write attribute " << name
                 << " failed: " << error;
    return false;
  }
  return true;
}

iio_device* IioDeviceImpl::GetUnderlyingIioDevice() const {
  return device_;
}

bool IioDeviceImpl::SetTrigger(IioDevice* trigger_device) {
  auto impl_device = trigger_device->GetUnderlyingIioDevice();
  if (!impl_device) {
    LOG(WARNING) << "cannot find device " << trigger_device->GetId()
                 << " in the current context";
    return false;
  }
  int ok = iio_device_set_trigger(device_, impl_device);
  if (ok) {
    LOG(WARNING) << "Unable to set trigger for device " << GetId()
                 << " to be device " << trigger_device->GetId()
                 << ", error: " << ok;
    return false;
  }
  return true;
}

IioDevice* IioDeviceImpl::GetTrigger() {
  const iio_device* trigger;
  int error = iio_device_get_trigger(device_, &trigger);
  if (error) {
    LOG(WARNING) << "Unable to get trigger for device " << GetId();
    return nullptr;
  }
  const char* trigger_id = iio_device_get_id(trigger);
  auto trigger_device = GetContext()->GetDevice(trigger_id);
  if (trigger_device == nullptr) {
    LOG(WARNING) << GetId() << " has trigger device " << trigger_id
                 << "which cannot be found in this context";
    return nullptr;
  }
  return trigger_device;
}

IioChannel* IioDeviceImpl::GetChannel(const std::string& name) {
  auto k = channels_.find(name);
  if (k != channels_.end())
    return k->second.get();
  iio_channel* channel = iio_device_find_channel(device_, name.c_str(), true);
  if (channel == nullptr)
    channel = iio_device_find_channel(device_, name.c_str(), false);
  if (channel == nullptr)
    return nullptr;
  channels_.emplace(name, std::make_unique<IioChannelImpl>(channel));
  return channels_[name].get();
}

bool IioDeviceImpl::EnableBuffer(size_t count) {
  if (!WriteNumberAttribute("buffer/length", count))
    return false;
  if (!WriteNumberAttribute("buffer/enable", 1))
    return false;

  return true;
}

bool IioDeviceImpl::DisableBuffer() {
  return WriteNumberAttribute("buffer/enable", 0);
}

bool IioDeviceImpl::IsBufferEnabled(size_t* count) const {
  bool enabled = (ReadNumberAttribute("buffer/enable").value_or(0) == 1);
  if (enabled && count)
    *count = ReadNumberAttribute("buffer/length").value_or(0);

  return enabled;
}

}  // namespace mems_setup
