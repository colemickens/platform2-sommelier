// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <sstream>

#include <base/bind.h>
#include <base/callback.h>
#include <base/logging.h>
#include <base/memory/weak_ptr.h>
#include <base/run_loop.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_split.h>
#include <base/strings/string_tokenizer.h>
#include <base/strings/string_util.h>
#include <base/threading/thread_task_runner_handle.h>
#include <base/time/time.h>
#include <re2/re2.h>

#include "diagnostics/telem/telem_connection.h"
#include "diagnostics/telem/telemetry_group_enum.h"
#include "diagnosticsd.pb.h"  // NOLINT(build/include)

namespace diagnostics {

namespace {
// Generic callback which moves the response to its destination. All
// specific parsing logic is left to other methods, allowing this
// single callback to be used for each gRPC request.
template <typename ResponseType>
void OnRpcResponseReceived(std::unique_ptr<ResponseType>* response_destination,
                           base::Closure run_loop_quit_closure,
                           std::unique_ptr<ResponseType> response) {
  *response_destination = std::move(response);
  run_loop_quit_closure.Run();
}

// Mapping between groups and items. Each item belongs to exactly one
// group.
const std::map<TelemetryGroupEnum, std::vector<TelemetryItemEnum>> group_map = {
    {TelemetryGroupEnum::kDisk,
     {TelemetryItemEnum::kMemTotalMebibytes,
      TelemetryItemEnum::kMemFreeMebibytes}}};

}  // namespace

TelemConnection::TelemConnection() {
  owned_client_impl_ = std::make_unique<AsyncGrpcClientAdapterImpl>();
  client_impl_ = owned_client_impl_.get();
}

TelemConnection::TelemConnection(AsyncGrpcClientAdapter* client)
    : client_impl_(client) {}

TelemConnection::~TelemConnection() {
  ShutdownClient();
}

void TelemConnection::Connect(const std::string& target_uri) {
  DCHECK(!client_impl_->IsConnected());

  client_impl_->Connect(target_uri);
}

const base::Optional<base::Value> TelemConnection::GetItem(
    TelemetryItemEnum item, base::TimeDelta acceptable_age) {
  // First, check to see if the desired telemetry information is
  // present and valid in the cache. If so, just return it.
  if (!cache_.IsValid(item, acceptable_age)) {
    // When no valid cached data is present, make a gRPC request to
    // obtain the appropriate telemetry data. This may result in
    // more data being fetched and cached than just the desired item.
    switch (item) {
      case TelemetryItemEnum::kUptime:
        UpdateProcData(grpc_api::GetProcDataRequest::FILE_UPTIME);
        break;
      case TelemetryItemEnum::kMemTotalMebibytes:  // FALLTHROUGH
      case TelemetryItemEnum::kMemFreeMebibytes:
        UpdateProcData(grpc_api::GetProcDataRequest::FILE_MEMINFO);
        break;
      case TelemetryItemEnum::kNumRunnableEntities:  // FALLTHROUGH
      case TelemetryItemEnum::kNumExistingEntities:
        UpdateProcData(grpc_api::GetProcDataRequest::FILE_LOADAVG);
        break;
      case TelemetryItemEnum::kTotalIdleTimeUserHz:  // FALLTHROUGH
      case TelemetryItemEnum::kIdleTimePerCPUUserHz:
        UpdateProcData(grpc_api::GetProcDataRequest::FILE_STAT);
        break;
      case TelemetryItemEnum::kAcpiButton:
        UpdateProcData(grpc_api::GetProcDataRequest::DIRECTORY_ACPI_BUTTON);
        break;
      case TelemetryItemEnum::kNetStat:
        UpdateProcData(grpc_api::GetProcDataRequest::FILE_NET_NETSTAT);
        break;
      case TelemetryItemEnum::kNetDev:
        UpdateProcData(grpc_api::GetProcDataRequest::FILE_NET_DEV);
        break;
    }
  }

  return cache_.GetParsedData(item);
}

const std::vector<
    std::pair<TelemetryItemEnum, const base::Optional<base::Value>>>
TelemConnection::GetGroup(TelemetryGroupEnum group,
                          base::TimeDelta acceptable_age) {
  std::vector<std::pair<TelemetryItemEnum, const base::Optional<base::Value>>>
      telem_items;

  for (TelemetryItemEnum item : group_map.at(group))
    telem_items.emplace_back(item, GetItem(item, acceptable_age));

  return telem_items;
}

// Sends a GetProcDataRequest, then parses and caches the response.
// If the response format for a specific telemetry item is incorrectly
// formatted, that item will be ignored and not cached, but other,
// correctly formatted telemetry items from the same response will
// still be parsed and cached.
void TelemConnection::UpdateProcData(grpc_api::GetProcDataRequest::Type type) {
  grpc_api::GetProcDataRequest request;
  request.set_type(type);
  std::unique_ptr<grpc_api::GetProcDataResponse> response;
  base::RunLoop run_loop;

  client_impl_->GetProcData(
      request, base::Bind(&OnRpcResponseReceived<grpc_api::GetProcDataResponse>,
                          base::Unretained(&response), run_loop.QuitClosure()));
  VLOG(0) << "Sent GetProcDataRequest";
  run_loop.Run();

  if (!response) {
    LOG(ERROR) << "No ProcDataResponse received.";
    return;
  }

  // Parse the response and cache the parsed data.
  switch (type) {
    case grpc_api::GetProcDataRequest::FILE_UPTIME:
      ExtractDataFromProcUptime(*response);
      break;
    case grpc_api::GetProcDataRequest::FILE_MEMINFO:
      ExtractDataFromProcMeminfo(*response);
      break;
    case grpc_api::GetProcDataRequest::FILE_LOADAVG:
      ExtractDataFromProcLoadavg(*response);
      break;
    case grpc_api::GetProcDataRequest::FILE_STAT:
      ExtractDataFromProcStat(*response);
      break;
    case grpc_api::GetProcDataRequest::DIRECTORY_ACPI_BUTTON:
      ExtractDataFromProcAcpiButton(*response);
      break;
    case grpc_api::GetProcDataRequest::FILE_NET_NETSTAT:
      ExtractDataFromProcNetStat(*response);
      break;
    case grpc_api::GetProcDataRequest::FILE_NET_DEV:
      ExtractDataFromProcNetDev(*response);
      break;
    default:
      LOG(ERROR) << "Bad ProcDataRequest type: " << type;
      return;
  }
}

void TelemConnection::ShutdownClient() {
  base::RunLoop loop;
  client_impl_->Shutdown(loop.QuitClosure());
  loop.Run();
}

// Parse the raw /proc/meminfo file.
void TelemConnection::ExtractDataFromProcMeminfo(
    const grpc_api::GetProcDataResponse& response) {
  // Make sure we received exactly one file in the response.
  if (response.file_dump_size() != 1) {
    LOG(ERROR) << "Bad GetProcDataResponse from request of type FILE_MEMINFO.";
    return;
  }

  // Parse the meminfo response for kMemTotal and kMemFree.
  base::StringPairs keyVals;
  base::SplitStringIntoKeyValuePairs(response.file_dump(0).contents(), ':',
                                     '\n', &keyVals);

  for (int i = 0; i < keyVals.size(); i++) {
    if (keyVals[i].first == "MemTotal") {
      // Convert from kB to MB and cache the result.
      int memtotal_mb;
      base::StringTokenizer t(keyVals[i].second, " ");
      if (t.GetNext() && base::StringToInt(t.token(), &memtotal_mb) &&
          t.GetNext() && t.token() == "kB") {
        memtotal_mb /= 1024;
        cache_.SetParsedData(
            TelemetryItemEnum::kMemTotalMebibytes,
            base::Optional<base::Value>(base::Value(memtotal_mb)));
      } else {
        LOG(ERROR) << "Incorrectly formatted MemTotal.";
      }
    } else if (keyVals[i].first == "MemFree") {
      // Convert from kB to MB and cache the result.
      int memfree_mb;
      base::StringTokenizer t(keyVals[i].second, " ");
      if (t.GetNext() && base::StringToInt(t.token(), &memfree_mb) &&
          t.GetNext() && t.token() == "kB") {
        memfree_mb /= 1024;
        cache_.SetParsedData(
            TelemetryItemEnum::kMemFreeMebibytes,
            base::Optional<base::Value>(base::Value(memfree_mb)));
      } else {
        LOG(ERROR) << "Incorrectly formatted MemFree.";
      }
    }
  }
}

void TelemConnection::ExtractDataFromProcUptime(
    const grpc_api::GetProcDataResponse& response) {
  NOTIMPLEMENTED();
}

void TelemConnection::ExtractDataFromProcLoadavg(
    const grpc_api::GetProcDataResponse& response) {
  // Make sure we received exactly one file in the response.
  if (response.file_dump_size() != 1) {
    LOG(ERROR) << "Bad GetProcDataResponse from request of type FILE_LOADAVG.";
    return;
  }

  // We expect the loadavg response to have the following format:
  // %f %f %f %d/%d %d. At the moment, we're only interested in
  // the %d/%d: it will parse into kNumRunnableEntities/kNumExistingEntities.
  // We'll first make sure the entire response matches the expected format,
  // then we'll extract the %d/%d.
  int running_entities;
  int existing_entities;
  if (!RE2::FullMatch(response.file_dump(0).contents(),
                      "\\d+\\.\\d+\\s\\d+\\.\\d+\\s\\d+"
                      "\\.\\d+\\s(\\d+)/(\\d+)\\s\\d+\\n",
                      &running_entities, &existing_entities)) {
    LOG(ERROR) << "Incorrectly formatted loadavg.";
    return;
  }
  cache_.SetParsedData(
      TelemetryItemEnum::kNumRunnableEntities,
      base::Optional<base::Value>(base::Value(running_entities)));
  cache_.SetParsedData(
      TelemetryItemEnum::kNumExistingEntities,
      base::Optional<base::Value>(base::Value(existing_entities)));
}

void TelemConnection::ExtractDataFromProcStat(
    const grpc_api::GetProcDataResponse& response) {
  // Make sure we received exactly one file in the response.
  if (response.file_dump_size() != 1) {
    LOG(ERROR) << "Bad GetProcDataResponse from request of type FILE_LOADAVG.";
    return;
  }

  // Grab the idle time for all CPUs combined, as well as the idle time
  // for each logical CPU. We'll store each of the times as a string in
  // case the system has been on for long enough to overflow an int with
  // its idle time.
  std::string idle_time_combined;
  std::stringstream response_sstream(response.file_dump(0).contents());
  std::string current_line;
  // The first line should be: cpu %d %d %d %d ..., where the last number
  // is the idle time.
  if (!std::getline(response_sstream, current_line) ||
      !RE2::PartialMatch(current_line, "cpu\\s+\\d+ \\d+ \\d+ (\\d+)",
                         &idle_time_combined)) {
    LOG(ERROR) << "Incorrectly formatted stat.";
    return;
  }

  // The next N lines should be: cpu%d %d %d %d %d ..., where N is the number
  // of logical CPUs.
  std::string idle_time_current_cpu;
  base::ListValue logical_cpu_idle_time;
  while (std::getline(response_sstream, current_line) &&
         RE2::PartialMatch(current_line, "cpu\\d+ \\d+ \\d+ \\d+ (\\d+)",
                           &idle_time_current_cpu))
    logical_cpu_idle_time.AppendString(idle_time_current_cpu);

  cache_.SetParsedData(
      TelemetryItemEnum::kTotalIdleTimeUserHz,
      base::Optional<base::Value>(base::Value(idle_time_combined)));
  cache_.SetParsedData(TelemetryItemEnum::kIdleTimePerCPUUserHz,
                       base::Optional<base::Value>(logical_cpu_idle_time));
}

void TelemConnection::ExtractDataFromProcAcpiButton(
    const grpc_api::GetProcDataResponse& response) {
  NOTIMPLEMENTED();
}

void TelemConnection::ExtractDataFromProcNetStat(
    const grpc_api::GetProcDataResponse& response) {
  NOTIMPLEMENTED();
}

void TelemConnection::ExtractDataFromProcNetDev(
    const grpc_api::GetProcDataResponse& response) {
  NOTIMPLEMENTED();
}

}  // namespace diagnostics
