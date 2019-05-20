// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/cros_healthd/cros_healthd_mojo_service.h"

#include <string>
#include <utility>
#include <vector>

#include <base/files/scoped_file.h>
#include <base/logging.h>
#include <dbus/cros_healthd/dbus-constants.h>
#include <mojo/edk/embedder/embedder.h>

#include "mojo/cros_healthd_telemetry.mojom.h"

namespace diagnostics {
namespace mojo_ipc = ::chromeos::cros_healthd::mojom;

namespace {

// Convert from Mojo enums to the TelemetryItemEnums which are used internally.
TelemetryItemEnum GetTelemetryItemEnumFromMojoEnum(
    mojo_ipc::TelemetryItemEnum mojo_enum) {
  switch (mojo_enum) {
    case mojo_ipc::TelemetryItemEnum::kMemTotal:
      return TelemetryItemEnum::kMemTotalMebibytes;
    case mojo_ipc::TelemetryItemEnum::kMemFree:
      return TelemetryItemEnum::kMemFreeMebibytes;
    case mojo_ipc::TelemetryItemEnum::kNumRunnableEntities:
      return TelemetryItemEnum::kNumRunnableEntities;
    case mojo_ipc::TelemetryItemEnum::kNumExistingEntities:
      return TelemetryItemEnum::kNumExistingEntities;
    case mojo_ipc::TelemetryItemEnum::kTotalIdleTime:
      return TelemetryItemEnum::kTotalIdleTimeUserHz;
    case mojo_ipc::TelemetryItemEnum::kIdleTimePerCPU:
      return TelemetryItemEnum::kIdleTimePerCPUUserHz;
  }
}

// Convert from TelemetryItemEnums to Mojo enums for external use.
mojo_ipc::TelemetryItemEnum GetMojoEnumFromTelemetryItemEnum(
    TelemetryItemEnum telem_item) {
  switch (telem_item) {
    case TelemetryItemEnum::kMemTotalMebibytes:
      return mojo_ipc::TelemetryItemEnum::kMemTotal;
    case TelemetryItemEnum::kMemFreeMebibytes:
      return mojo_ipc::TelemetryItemEnum::kMemFree;
    case TelemetryItemEnum::kNumRunnableEntities:
      return mojo_ipc::TelemetryItemEnum::kNumRunnableEntities;
    case TelemetryItemEnum::kNumExistingEntities:
      return mojo_ipc::TelemetryItemEnum::kNumExistingEntities;
    case TelemetryItemEnum::kTotalIdleTimeUserHz:
      return mojo_ipc::TelemetryItemEnum::kTotalIdleTime;
    case TelemetryItemEnum::kIdleTimePerCPUUserHz:
      return mojo_ipc::TelemetryItemEnum::kIdleTimePerCPU;
    case TelemetryItemEnum::kBatteryCycleCount:    // FALLTHROUGH
    case TelemetryItemEnum::kBatteryVoltage:       // FALLTHROUGH
    case TelemetryItemEnum::kBatteryManufacturer:  // FALLTHROUGH
    case TelemetryItemEnum::kUptime:   // FALLTHROUGH
    case TelemetryItemEnum::kNetStat:  // FALLTHROUGH
    case TelemetryItemEnum::kNetDev:   // FALLTHROUGH
    case TelemetryItemEnum::kHwmon:    // FALLTHROUGH
    case TelemetryItemEnum::kThermal:  // FALLTHROUGH
    case TelemetryItemEnum::kDmiTables:
      NOTIMPLEMENTED();
      // We'll return a dummy value just to satisfy the compiler.
      return mojo_ipc::TelemetryItemEnum::kMemTotal;
  }
}

// Convert from Mojo enums to the TelemetryGroupEnums which are used internally.
TelemetryGroupEnum GetTelemetryGroupEnumFromMojoEnum(
    mojo_ipc::TelemetryGroupEnum mojo_enum) {
  switch (mojo_enum) {
    case mojo_ipc::TelemetryGroupEnum::kDisk:
      return TelemetryGroupEnum::kDisk;
  }
}

template <typename T, typename Ptr>
mojo_ipc::TelemetryItemDataPtr MarshallInt(
    const base::Value& value,
    void (mojo_ipc::TelemetryItemData::*setter)(Ptr)) {
  DCHECK(value.is_int());
  int x;
  value.GetAsInteger(&x);
  mojo_ipc::TelemetryItemDataPtr item_data = mojo_ipc::TelemetryItemData::New();
  (item_data.get()->*(setter))(T::New(x));
  return item_data;
}

template <typename T, typename Ptr>
mojo_ipc::TelemetryItemDataPtr MarshallStr(
    const base::Value& value,
    void (mojo_ipc::TelemetryItemData::*setter)(Ptr)) {
  DCHECK(value.is_string());
  std::string x;
  value.GetAsString(&x);
  mojo_ipc::TelemetryItemDataPtr item_data = mojo_ipc::TelemetryItemData::New();
  (item_data.get()->*(setter))(T::New(x));
  return item_data;
}

template <typename T, typename Ptr>
mojo_ipc::TelemetryItemDataPtr MarshallStrVector(
    const base::Value& value,
    void (mojo_ipc::TelemetryItemData::*setter)(Ptr)) {
  DCHECK(value.is_list());
  const base::ListValue* list;
  value.GetAsList(&list);
  std::vector<std::string> str_vector;
  for (int i = 0; i < list->GetSize(); i++) {
    const base::Value* item;
    list->Get(i, &item);
    DCHECK(item->is_string());
    std::string x;
    item->GetAsString(&x);
    str_vector.push_back(x);
  }
  mojo_ipc::TelemetryItemDataPtr item_data = mojo_ipc::TelemetryItemData::New();
  (item_data.get()->*(setter))(T::New(str_vector));
  return item_data;
}

// Construct a TelemetryItemData from a base::Value.
mojo_ipc::TelemetryItemDataPtr GetItemDataFromBaseValue(
    const base::Value value, mojo_ipc::TelemetryItemEnum item) {
  switch (item) {
    case mojo_ipc::TelemetryItemEnum::kMemTotal:
      return MarshallInt<mojo_ipc::MemTotal>(
          value, &mojo_ipc::TelemetryItemData::set_mem_total);
    case mojo_ipc::TelemetryItemEnum::kMemFree:
      return MarshallInt<mojo_ipc::MemFree>(
          value, &mojo_ipc::TelemetryItemData::set_mem_free);
    case mojo_ipc::TelemetryItemEnum::kNumRunnableEntities:
      return MarshallInt<mojo_ipc::NumRunnableEntities>(
          value, &mojo_ipc::TelemetryItemData::set_num_runnable_entities);
    case mojo_ipc::TelemetryItemEnum::kNumExistingEntities:
      return MarshallInt<mojo_ipc::NumExistingEntities>(
          value, &mojo_ipc::TelemetryItemData::set_num_existing_entities);
    case mojo_ipc::TelemetryItemEnum::kTotalIdleTime:
      return MarshallStr<mojo_ipc::TotalIdleTime>(
          value, &mojo_ipc::TelemetryItemData::set_total_idle_time);
    case mojo_ipc::TelemetryItemEnum::kIdleTimePerCPU:
      return MarshallStrVector<mojo_ipc::IdleTimePerCPU>(
          value, &mojo_ipc::TelemetryItemData::set_idle_time_per_cpu);
  }
}

}  // namespace

CrosHealthdMojoService::CrosHealthdMojoService(
    CrosHealthdRoutineService* routine_service,
    CrosHealthdTelemetryService* telemetry_service,
    mojo::ScopedMessagePipeHandle mojo_pipe_handle)
    : routine_service_(routine_service),
      telemetry_service_(telemetry_service),
      self_binding_(
          std::make_unique<mojo::Binding<mojo_ipc::CrosHealthdService>>(
              this /* impl */, std::move(mojo_pipe_handle))) {
  DCHECK(routine_service_);
  DCHECK(telemetry_service_);
  DCHECK(self_binding_);
  DCHECK(self_binding_->is_bound());
}

CrosHealthdMojoService::~CrosHealthdMojoService() = default;

void CrosHealthdMojoService::GetTelemetryItem(
    mojo_ipc::TelemetryItemEnum item,
    const GetTelemetryItemCallback& callback) {
  base::Optional<base::Value> item_data = telemetry_service_->GetTelemetryItem(
      GetTelemetryItemEnumFromMojoEnum(item));
  if (!item_data) {
    LOG(ERROR) << "Failed to retrieve data from telemetry service.";
    callback.Run(mojo_ipc::TelemetryItemDataPtr());
    return;
  }
  callback.Run(GetItemDataFromBaseValue(*item_data, item));
}

void CrosHealthdMojoService::GetTelemetryGroup(
    mojo_ipc::TelemetryGroupEnum group,
    const GetTelemetryGroupCallback& callback) {
  std::vector<std::pair<TelemetryItemEnum, base::Optional<base::Value>>>
      telem_group_data = telemetry_service_->GetTelemetryGroup(
          GetTelemetryGroupEnumFromMojoEnum(group));
  std::vector<mojo_ipc::TelemetryItemWithValuePtr> mojo_group_data;
  for (auto pair : telem_group_data) {
    if (!pair.second)
      continue;
    mojo_ipc::TelemetryItemEnum mojo_item =
        GetMojoEnumFromTelemetryItemEnum(pair.first);
    mojo_group_data.push_back(mojo_ipc::TelemetryItemWithValue::New(
        mojo_item, GetItemDataFromBaseValue(*pair.second, mojo_item)));
  }
  callback.Run(std::move(mojo_group_data));
}

void CrosHealthdMojoService::GetAvailableRoutines(
    const GetAvailableRoutinesCallback& callback) {
  NOTIMPLEMENTED();
}

void CrosHealthdMojoService::RunRoutine(mojo_ipc::DiagnosticRoutineEnum routine,
                                        mojo_ipc::RoutineParametersPtr params,
                                        const RunRoutineCallback& callback) {
  NOTIMPLEMENTED();
}

void CrosHealthdMojoService::GetRoutineUpdate(
    int32_t uuid,
    mojo_ipc::DiagnosticRoutineCommandEnum command,
    bool include_output,
    const GetRoutineUpdateCallback& callback) {
  NOTIMPLEMENTED();
}

void CrosHealthdMojoService::set_connection_error_handler(
    const base::Closure& error_handler) {
  DCHECK(self_binding_);
  self_binding_->set_connection_error_handler(error_handler);
}

}  // namespace diagnostics
