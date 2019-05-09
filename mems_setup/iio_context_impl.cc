// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include <base/logging.h>

#include "mems_setup/iio_channel_impl.h"
#include "mems_setup/iio_context_impl.h"
#include "mems_setup/iio_device_impl.h"

namespace mems_setup {

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

IioDevice* IioContextImpl::GetDevice(const std::string& name) {
  auto k = devices_.find(name);
  if (k != devices_.end())
    return k->second.get();
  iio_device* device =
      iio_context_find_device(GetCurrentContext(), name.c_str());
  if (device == nullptr)
    return nullptr;
  devices_.emplace(name, std::make_unique<IioDeviceImpl>(this, device));
  return devices_[name].get();
}

}  // namespace mems_setup
