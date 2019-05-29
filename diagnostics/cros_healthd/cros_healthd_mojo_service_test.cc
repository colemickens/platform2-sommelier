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
#include "diagnostics/cros_healthd/cros_healthd_telemetry_service.h"
#include "diagnostics/telem/telemetry_item_enum.h"
#include "mojo/cros_healthd.mojom.h"

using testing::ByRef;
using testing::ElementsAreArray;
using testing::Return;
using testing::StrictMock;

namespace diagnostics {
namespace mojo_ipc = ::chromeos::cros_healthd::mojom;

namespace {

constexpr int kFakeIntValue = 13;
constexpr int kFakeMemFreeValue = 10;
constexpr int kFakeMemTotalValue = 11;
constexpr char kFakeStrValue[] = "Some string value.";
char const* kFakeStrValues[] = {"These", "are", "some", "values"};

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

class MockCrosHealthdTelemetryService : public CrosHealthdTelemetryService {
 public:
  MOCK_METHOD1(GetTelemetryItem,
               base::Optional<base::Value>(TelemetryItemEnum item));
  MOCK_METHOD1(
      GetTelemetryGroup,
      std::vector<std::pair<TelemetryItemEnum, base::Optional<base::Value>>>(
          TelemetryGroupEnum group));
};

// Tests for the CrosHealthddMojoService class.
class CrosHealthdMojoServiceTest : public testing::Test {
 protected:
  CrosHealthdMojoServiceTest() {
    mojo::edk::Init();
    service_ = std::make_unique<CrosHealthdMojoService>(
        &routine_service_, &telemetry_service_,
        mojo::MakeRequest(&service_ptr_).PassMessagePipe());
  }

  MockCrosHealthdRoutineService* routine_service() { return &routine_service_; }
  MockCrosHealthdTelemetryService* telemetry_service() {
    return &telemetry_service_;
  }

  void GetTelemetryItem(mojo_ipc::TelemetryItemEnum item,
                        const mojo_ipc::TelemetryItemDataPtr& expected_data) {
    base::RunLoop run_loop;
    service_ptr_->GetTelemetryItem(
        item, base::Bind(
                  [](const base::Closure& quit_closure,
                     const mojo_ipc::TelemetryItemDataPtr& expected_data,
                     mojo_ipc::TelemetryItemDataPtr actual_data) {
                    EXPECT_EQ(expected_data, actual_data);
                    quit_closure.Run();
                  },
                  run_loop.QuitClosure(), std::cref(expected_data)));
    run_loop.Run();
  }

  void GetTelemetryGroup(
      mojo_ipc::TelemetryGroupEnum group,
      const std::vector<mojo_ipc::TelemetryItemWithValuePtr>& expected_data) {
    base::RunLoop run_loop;
    service_ptr_->GetTelemetryGroup(
        group,
        base::Bind(
            [](const base::Closure& quit_closure,
               const std::vector<mojo_ipc::TelemetryItemWithValuePtr>&
                   expected_data,
               std::vector<mojo_ipc::TelemetryItemWithValuePtr> actual_data) {
              ASSERT_EQ(actual_data.size(), expected_data.size());
              for (int i = 0; i < actual_data.size(); i++)
                EXPECT_EQ(actual_data[i], expected_data[i]);
              quit_closure.Run();
            },
            run_loop.QuitClosure(), std::cref(expected_data)));
    run_loop.Run();
  }

 private:
  base::MessageLoop message_loop_;
  StrictMock<MockCrosHealthdRoutineService> routine_service_;
  StrictMock<MockCrosHealthdTelemetryService> telemetry_service_;
  mojo_ipc::CrosHealthdServicePtr service_ptr_;
  std::unique_ptr<CrosHealthdMojoService> service_;
};

}  // namespace

// Test that we handle the case where the telemetry service doesn't return data.
TEST_F(CrosHealthdMojoServiceTest, NoTelemetryDataFromService) {
  EXPECT_CALL(*telemetry_service(),
              GetTelemetryItem(TelemetryItemEnum::kMemTotalMebibytes))
      .WillOnce(Return(base::Optional<base::Value>()));
  mojo_ipc::TelemetryItemDataPtr data_ptr = mojo_ipc::TelemetryItemDataPtr();
  GetTelemetryItem(mojo_ipc::TelemetryItemEnum::kMemTotal, data_ptr);
}

// Test that we can retrieve kMemTotal.
TEST_F(CrosHealthdMojoServiceTest, GetMemTotal) {
  EXPECT_CALL(*telemetry_service(),
              GetTelemetryItem(TelemetryItemEnum::kMemTotalMebibytes))
      .WillOnce(
          Return(base::Optional<base::Value>(base::Value(kFakeIntValue))));
  mojo_ipc::TelemetryItemDataPtr data_ptr = mojo_ipc::TelemetryItemData::New();
  data_ptr->set_mem_total(mojo_ipc::MemTotal::New(kFakeIntValue));
  GetTelemetryItem(mojo_ipc::TelemetryItemEnum::kMemTotal, data_ptr);
}

// Test that we can retrieve kMemFree.
TEST_F(CrosHealthdMojoServiceTest, GetMemFree) {
  EXPECT_CALL(*telemetry_service(),
              GetTelemetryItem(TelemetryItemEnum::kMemFreeMebibytes))
      .WillOnce(
          Return(base::Optional<base::Value>(base::Value(kFakeIntValue))));
  mojo_ipc::TelemetryItemDataPtr data_ptr = mojo_ipc::TelemetryItemData::New();
  data_ptr->set_mem_free(mojo_ipc::MemFree::New(kFakeIntValue));
  GetTelemetryItem(mojo_ipc::TelemetryItemEnum::kMemFree, data_ptr);
}

// Test that we can retrieve kNumRunnableEntities.
TEST_F(CrosHealthdMojoServiceTest, GetNumRunnableEntities) {
  EXPECT_CALL(*telemetry_service(),
              GetTelemetryItem(TelemetryItemEnum::kNumRunnableEntities))
      .WillOnce(
          Return(base::Optional<base::Value>(base::Value(kFakeIntValue))));
  mojo_ipc::TelemetryItemDataPtr data_ptr = mojo_ipc::TelemetryItemData::New();
  data_ptr->set_num_runnable_entities(
      mojo_ipc::NumRunnableEntities::New(kFakeIntValue));
  GetTelemetryItem(mojo_ipc::TelemetryItemEnum::kNumRunnableEntities, data_ptr);
}

// Test that we can retrieve kNumExistingEntities.
TEST_F(CrosHealthdMojoServiceTest, GetNumExistingEntities) {
  EXPECT_CALL(*telemetry_service(),
              GetTelemetryItem(TelemetryItemEnum::kNumExistingEntities))
      .WillOnce(
          Return(base::Optional<base::Value>(base::Value(kFakeIntValue))));
  mojo_ipc::TelemetryItemDataPtr data_ptr = mojo_ipc::TelemetryItemData::New();
  data_ptr->set_num_existing_entities(
      mojo_ipc::NumExistingEntities::New(kFakeIntValue));
  GetTelemetryItem(mojo_ipc::TelemetryItemEnum::kNumExistingEntities, data_ptr);
}

// Test that we can retrieve kTotalIdleTime.
TEST_F(CrosHealthdMojoServiceTest, GetTotalIdleTime) {
  EXPECT_CALL(*telemetry_service(),
              GetTelemetryItem(TelemetryItemEnum::kTotalIdleTimeUserHz))
      .WillOnce(
          Return(base::Optional<base::Value>(base::Value(kFakeStrValue))));
  mojo_ipc::TelemetryItemDataPtr data_ptr = mojo_ipc::TelemetryItemData::New();
  data_ptr->set_total_idle_time(mojo_ipc::TotalIdleTime::New(kFakeStrValue));
  GetTelemetryItem(mojo_ipc::TelemetryItemEnum::kTotalIdleTime, data_ptr);
}

// Test that we can retrieve kIdleTimePerCPU.
TEST_F(CrosHealthdMojoServiceTest, GetIdleTimePerCPU) {
  base::ListValue list;
  for (auto str : kFakeStrValues)
    list.AppendString(str);
  EXPECT_CALL(*telemetry_service(),
              GetTelemetryItem(TelemetryItemEnum::kIdleTimePerCPUUserHz))
      .WillOnce(Return(base::Optional<base::Value>(list)));
  mojo_ipc::TelemetryItemDataPtr data_ptr = mojo_ipc::TelemetryItemData::New();
  data_ptr->set_idle_time_per_cpu(
      mojo_ipc::IdleTimePerCPU::New(std::vector<std::string>(
          kFakeStrValues, kFakeStrValues + sizeof(kFakeStrValues) /
                                               sizeof(kFakeStrValues[0]))));
  GetTelemetryItem(mojo_ipc::TelemetryItemEnum::kIdleTimePerCPU, data_ptr);
}

// Test that we can retrieve kMemory.
TEST_F(CrosHealthdMojoServiceTest, GetDiskGroup) {
  EXPECT_CALL(*telemetry_service(),
              GetTelemetryGroup(TelemetryGroupEnum::kMemory))
      .WillOnce(
          Return(std::vector<
                 std::pair<TelemetryItemEnum, base::Optional<base::Value>>>{
              {TelemetryItemEnum::kMemTotalMebibytes,
               base::Optional<base::Value>(base::Value(kFakeMemTotalValue))},
              {TelemetryItemEnum::kMemFreeMebibytes,
               base::Optional<base::Value>(base::Value(kFakeMemFreeValue))}}));
  mojo_ipc::TelemetryItemData mem_total;
  mem_total.set_mem_total(mojo_ipc::MemTotal::New(kFakeMemTotalValue));
  mojo_ipc::TelemetryItemData mem_free;
  mem_free.set_mem_free(mojo_ipc::MemFree::New(kFakeMemFreeValue));
  std::vector<mojo_ipc::TelemetryItemWithValuePtr> expected_data;
  expected_data.push_back(mojo_ipc::TelemetryItemWithValue::New(
      mojo_ipc::TelemetryItemEnum::kMemTotal, mem_total.Clone()));
  expected_data.push_back(mojo_ipc::TelemetryItemWithValue::New(
      mojo_ipc::TelemetryItemEnum::kMemFree, mem_free.Clone()));
  GetTelemetryGroup(mojo_ipc::TelemetryGroupEnum::kMemory, expected_data);
}

}  // namespace diagnostics
