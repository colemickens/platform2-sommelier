// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_TELEM_TELEM_CONNECTION_H_
#define DIAGNOSTICS_TELEM_TELEM_CONNECTION_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <base/macros.h>
#include <base/optional.h>
#include <base/time/time.h>
#include <base/values.h>

#include "diagnostics/telem/async_grpc_client_adapter.h"
#include "diagnostics/telem/async_grpc_client_adapter_impl.h"
#include "diagnostics/telem/telem_cache.h"
#include "diagnostics/telem/telemetry_group_enum.h"
#include "diagnostics/telem/telemetry_item_enum.h"
#include "diagnosticsd.grpc.pb.h"  // NOLINT(build/include)

namespace diagnostics {

// Libtelem's main interface for requesting telemetry information.
//
// Example usage:
//   TelemConnection telem_connection(grpc_uri);
//   telem_connection.Connect();
//   const base::Value* memtotal_mb =
//     telem_connection.GetItem(TelemetryItemEnum::kMemTotalMB);
class TelemConnection final {
 public:
  // Creates a TelemConnection object which can listen over the
  // gRPC URI |target_uri|. All production code should use this
  // constructor.
  TelemConnection();

  // Injects a custom implementation of the AsyncGrpcClientAdapter interface.
  // This constructor should only be used for testing.
  explicit TelemConnection(AsyncGrpcClientAdapter* client);
  ~TelemConnection();

  // Opens a gRPC connection on the interface specified by |target_uri_|.
  // This method should only be called a single time for each TelemConnection
  // object.
  void Connect(const std::string& target_uri);

  // Returns telemetry data corresponding to |item|, which was updated at least
  // |acceptable_age| ago. Connect() must be called before any calls to this
  // method. The returned value should be checked before it is used - the
  // function will return base::nullopt if the requested item could not be
  // retrieved.
  const base::Optional<base::Value> GetItem(TelemetryItemEnum item,
                                            base::TimeDelta acceptable_age);

  // Returns telemetry data for each item in |group|, which was updated at least
  // |acceptable_age| ago. Connect() must be called before any calls to this
  // method.
  const std::vector<
      std::pair<TelemetryItemEnum, const base::Optional<base::Value>>>
  GetGroup(TelemetryGroupEnum group, base::TimeDelta acceptable_age);

 private:
  // Updates the internal cache with new telemetry data from /proc/.
  void UpdateProcData(grpc_api::GetProcDataRequest::Type type);
  // Updates the internal cache with new telemetry data from /sys/.
  void UpdateSysfsData(grpc_api::GetSysfsDataRequest::Type type);

  // Gracefully shut down the AsyncGrpcClientAdapter.
  void ShutdownClient();

  // The following methods parse raw file dumps of various files under /proc/
  // for relevant telemetry information.
  void ExtractDataFromProcMeminfo(
      const grpc_api::GetProcDataResponse& response);
  void ExtractDataFromProcUptime(const grpc_api::GetProcDataResponse& response);
  void ExtractDataFromProcLoadavg(
      const grpc_api::GetProcDataResponse& response);
  void ExtractDataFromProcStat(const grpc_api::GetProcDataResponse& response);
  void ExtractDataFromProcAcpiButton(
      const grpc_api::GetProcDataResponse& response);
  void ExtractDataFromProcNetStat(
      const grpc_api::GetProcDataResponse& response);
  void ExtractDataFromProcNetDev(const grpc_api::GetProcDataResponse& response);

  // The following methods parse raw files dumps of various files under /sys/
  // for relevant telemetry information.
  void ExtractDataFromSysfsHwmon(
      const grpc_api::GetSysfsDataResponse& response);
  void ExtractDataFromSysfsThermal(
      const grpc_api::GetSysfsDataResponse& response);
  void ExtractDataFromSysfsDmiTables(
      const grpc_api::GetSysfsDataResponse& response);

  TelemCache cache_;
  // Owned default implementation, used when no AsyncGrpcClientAdapter
  // implementation is provided in the constructor.
  std::unique_ptr<AsyncGrpcClientAdapterImpl> owned_client_impl_;
  // Unowned pointer to the chosen AsyncGrpcClientAdapter implementation.
  AsyncGrpcClientAdapter* client_impl_;

  DISALLOW_COPY_AND_ASSIGN(TelemConnection);
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_TELEM_TELEM_CONNECTION_H_
