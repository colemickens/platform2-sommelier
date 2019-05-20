// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEMS_SETUP_TEST_HELPER_H_
#define MEMS_SETUP_TEST_HELPER_H_

#include <initializer_list>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include <gtest/gtest.h>

#include "mems_setup/configuration.h"
#include "mems_setup/delegate.h"
#include "mems_setup/iio_context.h"
#include "mems_setup/iio_device.h"
#include "mems_setup/sensor_kind.h"
#include "mems_setup/test_mocks.h"

namespace mems_setup {
namespace testing {

class MockSysfsTrigger : public mems_setup::mocks::MockIioDevice {
 public:
  MockSysfsTrigger(mems_setup::mocks::MockIioContext* ctx,
                   mems_setup::mocks::MockIioDevice* trigger0)
      : MockIioDevice(ctx, "iio_sysfs_trigger", "iio_sysfs_trigger"),
        mock_context_(ctx),
        mock_trigger0_(trigger0) {}

  bool WriteNumberAttribute(const std::string& name, int64_t value) override;

 private:
  mems_setup::mocks::MockIioContext* mock_context_;
  mems_setup::mocks::MockIioDevice* mock_trigger0_;
};

class SensorTestBase : public ::testing::Test {
 public:
  Configuration* GetConfiguration();

 protected:
  std::unique_ptr<mems_setup::mocks::MockIioContext> mock_context_;
  std::unique_ptr<mems_setup::mocks::MockDelegate> mock_delegate_;
  std::unique_ptr<mems_setup::mocks::MockIioDevice> mock_device_;

  std::unique_ptr<mems_setup::mocks::MockIioDevice> mock_trigger0_;
  std::unique_ptr<MockSysfsTrigger> mock_sysfs_trigger_;

  std::unique_ptr<Configuration> config_;

  SensorKind sensor_kind_;

  SensorTestBase(const char* name, const char* id, SensorKind kind);

  void SetSingleSensor(const char* location);

  void ConfigureVpd(
      std::initializer_list<std::pair<const char*, const char*>> values);
};

}  // namespace testing
}  // namespace mems_setup

#endif  // MEMS_SETUP_TEST_HELPER_H_
