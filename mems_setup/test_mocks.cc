// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/logging.h>

#include "mems_setup/test_mocks.h"

namespace mems_setup {
namespace mocks {

MockIioChannel::MockIioChannel(const std::string& id, bool enabled)
    : id_(id), enabled_(enabled) {}

bool MockIioChannel::SetEnabled(bool en) {
  enabled_ = en;
  return true;
}

MockIioDevice::MockIioDevice(MockIioContext* ctx,
                             const std::string& name,
                             const std::string& id)
    : IioDevice(), context_(ctx), name_(name), id_(id) {}

base::Optional<std::string> MockIioDevice::ReadStringAttribute(
    const std::string& name) const {
  auto k = text_attributes_.find(name), e = text_attributes_.end();
  if (k == e)
    return base::nullopt;
  return k->second;
}
base::Optional<int64_t> MockIioDevice::ReadNumberAttribute(
    const std::string& name) const {
  auto k = numeric_attributes_.find(name), e = numeric_attributes_.end();
  if (k == e)
    return base::nullopt;
  return k->second;
}

bool MockIioDevice::WriteStringAttribute(const std::string& name,
                                         const std::string& value) {
  text_attributes_[name] = value;
  return true;
}
bool MockIioDevice::WriteNumberAttribute(const std::string& name,
                                         int64_t value) {
  numeric_attributes_[name] = value;
  return true;
}

bool MockIioDevice::SetTrigger(IioDevice* trigger) {
  trigger_ = trigger;
  return true;
}

IioChannel* MockIioDevice::GetChannel(const std::string& id) {
  auto k = channels_.find(id), e = channels_.end();
  if (k == e)
    return nullptr;
  return k->second;
}

bool MockIioDevice::EnableBuffer(size_t n) {
  buffer_length_ = n;
  buffer_enabled_ = true;
  return true;
}
bool MockIioDevice::DisableBuffer() {
  buffer_enabled_ = false;
  return true;
}
bool MockIioDevice::IsBufferEnabled(size_t* n) const {
  if (n)
    *n = buffer_length_;
  return buffer_enabled_;
}

void MockIioContext::AddDevice(MockIioDevice* device) {
  CHECK(device);
  devices_.emplace(device->GetName(), device);
  devices_.emplace(device->GetId(), device);
}

IioDevice* MockIioContext::GetDevice(const std::string& name) {
  auto k = devices_.find(name);
  return (k == devices_.end()) ? nullptr : k->second;
}

base::Optional<std::string> MockDelegate::ReadVpdValue(
    const std::string& name) {
  auto k = vpd_.find(name);
  if (k == vpd_.end())
    return base::nullopt;
  return k->second;
}

bool MockDelegate::ProbeKernelModule(const std::string& module) {
  probed_modules_.push_back(module);
  return true;
}

bool MockDelegate::Exists(const base::FilePath& fp) {
  return false;
}

base::Optional<gid_t> MockDelegate::FindGroupId(const char* group) {
  auto k = groups_.find(group);
  if (k == groups_.end())
    return base::nullopt;
  return k->second;
}

int MockDelegate::GetPermissions(const base::FilePath& path) {
  auto k = permissions_.find(path.value());
  if (k == permissions_.end())
    return 0;
  return k->second;
}

bool MockDelegate::SetPermissions(const base::FilePath& path, int mode) {
  permissions_[path.value()] = mode;
  return true;
}

bool MockDelegate::GetOwnership(const base::FilePath& path,
                                uid_t* user,
                                gid_t* group) {
  auto k = ownerships_.find(path.value());
  if (k == ownerships_.end())
    return false;
  if (user)
    *user = k->second.first;
  if (group)
    *group = k->second.second;
  return true;
}

bool MockDelegate::SetOwnership(const base::FilePath& path,
                                uid_t user,
                                gid_t group) {
  ownerships_[path.value()] = {user, group};
  return true;
}

}  // namespace mocks
}  // namespace mems_setup
