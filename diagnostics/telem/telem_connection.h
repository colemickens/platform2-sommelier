// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_TELEM_TELEM_CONNECTION_H_
#define DIAGNOSTICS_TELEM_TELEM_CONNECTION_H_

#include <memory>
#include <string>

#include <base/macros.h>

#include "diagnostics/grpc_async_adapter/async_grpc_client.h"
#include "diagnosticsd.grpc.pb.h"  // NOLINT(build/include)

namespace diagnostics {

enum class TelemetryItemEnum {
  kMemInfo,
  KAcpiButton
  // TODO(pmoy@chromium.org): Add the rest of the telemetry items.
};

class TelemConnection {
 public:
  explicit TelemConnection(const std::string& target_uri);
  ~TelemConnection();
  // Opens a gRPC connection on the interface specified by |target_uri|.
  void Connect();
  void GetItem(TelemetryItemEnum item);

 private:
  void GetProcMessage(grpc_api::GetProcDataRequest::Type type);
  void GetProcFile(grpc_api::GetProcDataRequest::Type type);
  void GetProcDirectory(grpc_api::GetProcDataRequest::Type type);
  void ShutdownClient();

  const std::string target_uri_;

  std::unique_ptr<AsyncGrpcClient<grpc_api::Diagnosticsd>> client_;

  DISALLOW_COPY_AND_ASSIGN(TelemConnection);
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_TELEM_TELEM_CONNECTION_H_
