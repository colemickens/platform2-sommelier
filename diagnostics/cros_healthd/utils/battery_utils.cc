// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/cros_healthd/utils/battery_utils.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <base/bind.h>
#include <base/process/launch.h>
#include <base/files/file_util.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>
#include <base/time/time.h>
#include <base/values.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/bus.h>
#include <dbus/message.h>
#include <dbus/object_proxy.h>
#include <re2/re2.h>

#include "debugd/dbus-proxies.h"
#include "power_manager/proto_bindings/power_supply_properties.pb.h"

namespace diagnostics {

BatteryFetcher::BatteryFetcher(
    org::chromium::debugdProxyInterface* debugd_proxy,
    dbus::ObjectProxy* power_manager_proxy)
    : debugd_proxy_(debugd_proxy), power_manager_proxy_(power_manager_proxy) {}
BatteryFetcher::~BatteryFetcher() = default;

namespace {
constexpr char kManufactureDateSmart[] = "manufacture_date_smart";
constexpr char kTemperatureSmart[] = "temperature_smart";
enum class PipeState {
  PENDING,  // Bytes are currently being written to the destination string
  ERROR,    // Failed to succesfully read bytes from source file descriptor
  DONE      // All bytes were succesfully read from the source file descriptor
};

// The system-defined size of buffer used to read from a pipe.
const size_t kBufferSize = PIPE_BUF;
// Seconds to wait for debugd to return probe results.
const time_t kWaitSeconds = 5;

PipeState ReadPipe(int src_fd, std::string* dst_str) {
  char buffer[kBufferSize];
  const ssize_t bytes_read = HANDLE_EINTR(read(src_fd, buffer, kBufferSize));
  if (bytes_read < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
    PLOG(ERROR) << "read() from fd " << src_fd << " failed";
    return PipeState::ERROR;
  }
  if (bytes_read == 0) {
    return PipeState::DONE;
  }
  if (bytes_read > 0) {
    dst_str->append(buffer, bytes_read);
  }
  return PipeState::PENDING;
}

bool ReadNonblockingPipeToString(int fd, std::string* out) {
  fd_set read_fds;
  struct timeval timeout;

  FD_ZERO(&read_fds);
  FD_SET(fd, &read_fds);

  timeout.tv_sec = kWaitSeconds;
  timeout.tv_usec = 0;

  while (true) {
    int retval = select(fd + 1, &read_fds, nullptr, nullptr, &timeout);
    if (retval < 0) {
      PLOG(ERROR) << "select() failed from debugd";
      return false;
    }

    // Should only happen on timeout. Log a warning here, so we get at least a
    // log if the process is stale.
    if (retval == 0) {
      LOG(WARNING) << "select() timed out. Process might be stale.";
      return false;
    }

    PipeState state = ReadPipe(fd, out);
    if (state == PipeState::DONE) {
      return true;
    }
    if (state == PipeState::ERROR) {
      return false;
    }
  }
}

}  // namespace

template <typename T>
bool BatteryFetcher::FetchSmartBatteryMetric(
    const std::string& metric_name,
    T* smart_metric,
    base::OnceCallback<bool(const base::StringPiece& input, T* output)>
        convert_string_to_num) {
  constexpr auto kDebugdSmartBatteryFunction = "CollectSmartBatteryMetric";
  dbus::MethodCall method_call(debugd::kDebugdInterface,
                               kDebugdSmartBatteryFunction);

  dbus::MessageWriter writer(&method_call);
  writer.AppendString(metric_name);

  // In milliseconds, the time to wait for a debugd call.
  constexpr base::TimeDelta kDebugdTimeOut =
      base::TimeDelta::FromMilliseconds(10 * 1000);
  std::unique_ptr<dbus::Response> response =
      debugd_proxy_->GetObjectProxy()->CallMethodAndBlock(
          &method_call, kDebugdTimeOut.InMilliseconds());

  if (!response) {
    LOG(ERROR) << "Failed to issue D-Bus call to method "
               << kDebugdSmartBatteryFunction << " of debugd D-Bus interface";
    return false;
  }

  dbus::MessageReader reader(response.get());
  base::ScopedFD read_fd{};
  if (!reader.PopFileDescriptor(&read_fd)) {
    LOG(ERROR) << "Failed to read fd that represents the read end of the pipe"
                  " from debugd";
    return false;
  }

  std::string debugd_result;
  if (!ReadNonblockingPipeToString(read_fd.get(), &debugd_result)) {
    LOG(ERROR) << "Cannot read result from debugd";
    return false;
  }

  std::move(convert_string_to_num).Run(debugd_result, smart_metric);
  return true;
}

// Extract the battery metrics from the PowerSupplyProperties protobuf.
// Return true if the metrics could be successfully extracted from |response|
// and put into |output_info|.
bool BatteryFetcher::ExtractBatteryMetrics(dbus::Response* response,
                                           BatteryInfoPtr* output_info) {
  DCHECK(response);
  DCHECK(output_info);

  BatteryInfo info;
  power_manager::PowerSupplyProperties power_supply_proto;
  dbus::MessageReader reader_response(response);
  if (!reader_response.PopArrayOfBytesAsProto(&power_supply_proto)) {
    LOG(ERROR) << "Could not successfully read power supply protobuf";
    return false;
  }
  info.cycle_count = power_supply_proto.has_battery_cycle_count()
                         ? power_supply_proto.battery_cycle_count()
                         : 0;
  info.vendor = power_supply_proto.has_battery_vendor()
                    ? power_supply_proto.battery_vendor()
                    : "";
  info.voltage_now = power_supply_proto.has_battery_voltage()
                         ? power_supply_proto.battery_voltage()
                         : 0.0;
  info.charge_full = power_supply_proto.has_battery_charge_full()
                         ? power_supply_proto.battery_charge_full()
                         : 0.0;
  info.charge_full_design =
      power_supply_proto.has_battery_charge_full_design()
          ? power_supply_proto.battery_charge_full_design()
          : 0.0;
  info.serial_number = power_supply_proto.has_battery_serial_number()
                           ? power_supply_proto.battery_serial_number()
                           : "";
  info.voltage_min_design =
      power_supply_proto.has_battery_voltage_min_design()
          ? power_supply_proto.battery_voltage_min_design()
          : 0.0;
  info.model_name = power_supply_proto.has_battery_model_name()
                        ? power_supply_proto.battery_model_name()
                        : "";
  info.charge_now = power_supply_proto.has_battery_charge()
                        ? power_supply_proto.battery_charge()
                        : 0;
  int64_t manufacture_date_smart;
  base::OnceCallback<bool(const base::StringPiece& input, int64_t* output)>
      convert_string_to_int =
          base::BindOnce([](const base::StringPiece& input, int64_t* output) {
            return base::StringToInt64(input, output);
          });
  info.manufacture_date_smart =
      FetchSmartBatteryMetric<int64_t>(kManufactureDateSmart,
                                       &manufacture_date_smart,
                                       std::move(convert_string_to_int))
          ? manufacture_date_smart
          : 0;
  uint64_t temperature_smart;
  base::OnceCallback<bool(const base::StringPiece& input, uint64_t* output)>
      convert_string_to_uint =
          base::BindOnce([](const base::StringPiece& input, uint64_t* output) {
            return base::StringToUint64(input, output);
          });
  info.temperature_smart =
      FetchSmartBatteryMetric<uint64_t>(kTemperatureSmart, &temperature_smart,
                                        std::move(convert_string_to_uint))
          ? temperature_smart
          : 0;

  *output_info = info.Clone();

  return true;
}

// Make a D-Bus call to get the PowerSupplyProperties proto, which contains the
// battery metrics.
bool BatteryFetcher::FetchBatteryMetrics(BatteryInfoPtr* output_info) {
  constexpr base::TimeDelta kPowerManagerDBusTimeout =
      base::TimeDelta::FromSeconds(3);
  dbus::MethodCall method_call(power_manager::kPowerManagerInterface,
                               power_manager::kGetPowerSupplyPropertiesMethod);
  auto response = power_manager_proxy_->CallMethodAndBlock(
      &method_call, kPowerManagerDBusTimeout.InMilliseconds());
  return ExtractBatteryMetrics(response.get(), output_info);
}

std::vector<BatteryFetcher::BatteryInfoPtr> BatteryFetcher::FetchBatteryInfo() {
  // Since Chromebooks currently only support a single battery (main battery),
  // the vector should have a size of one. In the future, if Chromebooks
  // contain more batteries, they can easily be supported by the vector.
  std::vector<BatteryInfoPtr> batteries{};

  BatteryInfoPtr info;
  if (FetchBatteryMetrics(&info)) {
    batteries.push_back(std::move(info));
  }

  return batteries;
}

}  // namespace diagnostics
