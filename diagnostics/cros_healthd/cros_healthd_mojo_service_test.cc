// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include <base/bind.h>
#include <base/message_loop/message_loop.h>
#include <base/run_loop.h>
#include <dbus/mock_bus.h>
#include <dbus/mock_object_proxy.h>
#include <dbus/object_path.h>
#include <dbus/power_manager/dbus-constants.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <mojo/edk/embedder/embedder.h>

#include "debugd/dbus-proxy-mocks.h"
#include "diagnostics/cros_healthd/cros_healthd_mojo_service.h"
#include "diagnostics/cros_healthd/utils/battery_utils.h"
#include "mojo/cros_healthd.mojom.h"

using testing::ByRef;
using testing::ElementsAreArray;
using testing::Return;
using testing::StrictMock;

namespace diagnostics {
namespace mojo_ipc = ::chromeos::cros_healthd::mojom;

// Tests for the CrosHealthddMojoService class.
class CrosHealthdMojoServiceTest : public testing::Test {
 protected:
  CrosHealthdMojoServiceTest() {
    mojo::edk::Init();
    mock_debugd_proxy_ = std::make_unique<org::chromium::debugdProxyMock>();
    options_.bus_type = dbus::Bus::SYSTEM;
    mock_bus_ = new dbus::MockBus(options_);
    mock_power_manager_proxy_ = new dbus::MockObjectProxy(
        mock_bus_.get(), power_manager::kPowerManagerServiceName,
        dbus::ObjectPath(power_manager::kPowerManagerServicePath));
    battery_fetcher_ = std::make_unique<BatteryFetcher>(
        mock_debugd_proxy_.get(), mock_power_manager_proxy_.get());
    service_ = std::make_unique<CrosHealthdMojoService>(battery_fetcher_.get());
  }

 private:
  base::MessageLoop message_loop_;
  mojo_ipc::CrosHealthdServicePtr service_ptr_;
  std::unique_ptr<org::chromium::debugdProxyMock> mock_debugd_proxy_;
  dbus::Bus::Options options_;
  scoped_refptr<dbus::MockBus> mock_bus_;
  scoped_refptr<dbus::MockObjectProxy> mock_power_manager_proxy_;
  std::unique_ptr<BatteryFetcher> battery_fetcher_;
  std::unique_ptr<CrosHealthdMojoService> service_;
};

}  // namespace diagnostics
