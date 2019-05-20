// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mems_setup/test_helper.h"

using mems_setup::mocks::MockDelegate;
using mems_setup::mocks::MockIioChannel;
using mems_setup::mocks::MockIioContext;
using mems_setup::mocks::MockIioDevice;

namespace mems_setup {
namespace testing {

bool MockSysfsTrigger::WriteNumberAttribute(const std::string& name,
                                            int64_t value) {
  bool ok = this->MockIioDevice::WriteNumberAttribute(name, value);
  if (ok && name == "add_trigger" && value == 0) {
    mock_context_->AddDevice(mock_trigger0_);
  }
  return ok;
}

SensorTestBase::SensorTestBase(const char* name,
                               const char* id,
                               SensorKind kind)
    : mock_context_(new MockIioContext),
      mock_delegate_(new MockDelegate),
      mock_device_(
          std::make_unique<MockIioDevice>(mock_context_.get(), name, id)),
      mock_trigger0_(std::make_unique<MockIioDevice>(
          mock_context_.get(), "trigger0", "trigger0")),
      mock_sysfs_trigger_(std::make_unique<MockSysfsTrigger>(
          mock_context_.get(), mock_trigger0_.get())),
      sensor_kind_(kind) {
  mock_context_->AddDevice(mock_device_.get());
  mock_context_->AddDevice(mock_sysfs_trigger_.get());
}

void SensorTestBase::SetSingleSensor(const char* location) {
  mock_device_->WriteStringAttribute("location", location);
}

void SensorTestBase::ConfigureVpd(
    std::initializer_list<std::pair<const char*, const char*>> values) {
  for (const auto& value : values) {
    mock_delegate_->SetVpdValue(value.first, value.second);
  }
}

Configuration* SensorTestBase::GetConfiguration() {
  if (config_ == nullptr) {
    config_.reset(new Configuration(mock_device_.get(), sensor_kind_,
                                    mock_delegate_.get()));
  }

  return config_.get();
}

}  // namespace testing
}  // namespace mems_setup
