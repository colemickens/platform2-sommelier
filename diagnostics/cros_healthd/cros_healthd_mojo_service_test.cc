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
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <mojo/edk/embedder/embedder.h>

#include "diagnostics/cros_healthd/cros_healthd_mojo_service.h"
#include "diagnostics/cros_healthd/cros_healthd_routine_service.h"
#include "mojo/cros_healthd.mojom.h"

using testing::ByRef;
using testing::ElementsAreArray;
using testing::Return;
using testing::StrictMock;

namespace diagnostics {
namespace mojo_ipc = ::chromeos::cros_healthd::mojom;

namespace {

class MockCrosHealthdRoutineService : public CrosHealthdRoutineService {
 public:
  MOCK_METHOD0(GetAvailableRoutines,
               std::vector<mojo_ipc::DiagnosticRoutineEnum>());
  MOCK_METHOD4(RunBatteryRoutine,
               void(int low_mah,
                    int high_mah,
                    int* id,
                    mojo_ipc::DiagnosticRoutineStatusEnum* status));
  MOCK_METHOD4(RunBatterySysfsRoutine,
               void(int maximum_cycle_count,
                    int percent_battery_wear_allowed,
                    int* id,
                    mojo_ipc::DiagnosticRoutineStatusEnum* status));
  MOCK_METHOD3(RunUrandomRoutine,
               void(int length_seconds,
                    int* id,
                    mojo_ipc::DiagnosticRoutineStatusEnum* status));
  MOCK_METHOD4(
      GetRoutineUpdate,
      void(int32_t uuid,
           mojo_ipc::DiagnosticRoutineCommandEnum command,
           bool include_output,
           const mojo_ipc::CrosHealthdService::GetRoutineUpdateCallback&
               callback));
};

// Tests for the CrosHealthddMojoService class.
class CrosHealthdMojoServiceTest : public testing::Test {
 protected:
  CrosHealthdMojoServiceTest() {
    mojo::edk::Init();
    service_ = std::make_unique<CrosHealthdMojoService>(
        &routine_service_, mojo::MakeRequest(&service_ptr_).PassMessagePipe());
  }

  MockCrosHealthdRoutineService* routine_service() { return &routine_service_; }

 private:
  base::MessageLoop message_loop_;
  StrictMock<MockCrosHealthdRoutineService> routine_service_;
  mojo_ipc::CrosHealthdServicePtr service_ptr_;
  std::unique_ptr<CrosHealthdMojoService> service_;
};

}  // namespace

}  // namespace diagnostics
