// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_TELEM_ASYNC_GRPC_CLIENT_ADAPTER_H_
#define DIAGNOSTICS_TELEM_ASYNC_GRPC_CLIENT_ADAPTER_H_

#include <memory>
#include <string>

#include <base/callback.h>

#include "diagnosticsd.grpc.pb.h"  // NOLINT(build/include)

namespace diagnostics {

// Provides an interface for connecting to and communicating
// with the wilco_dtc_supportd daemon.
class AsyncGrpcClientAdapter {
 public:
  virtual ~AsyncGrpcClientAdapter() = default;

  // Whether or not the AsyncGrpcClientAdapter is currently
  // connected to the wilco_dtc_supportd daemon.
  virtual bool IsConnected() const = 0;

  // Connects to the wilco_dtc_supportd daemon. This method should
  // only be called a single time. The resulting connection
  // lasts for the lifetime of the AsyncGrpcClientAdapter.
  virtual void Connect(const std::string& target_uri) = 0;

  // Gracefully shutdown the connection to the wilco_dtc_supportd
  // daemon.
  virtual void Shutdown(const base::Closure& on_shutdown) = 0;

  // Get a raw dump of a file from /proc/. When the response
  // is ready, |callback| will be run.
  virtual void GetProcData(
      const grpc_api::GetProcDataRequest& request,
      base::Callback<void(std::unique_ptr<grpc_api::GetProcDataResponse>
                              response)> callback) = 0;

  // Get a raw dump of a file from /sys/. When the response
  // is ready, |callback| will be run.
  virtual void GetSysfsData(
      const grpc_api::GetSysfsDataRequest& request,
      base::Callback<void(std::unique_ptr<grpc_api::GetSysfsDataResponse>
                              response)> callback) = 0;
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_TELEM_ASYNC_GRPC_CLIENT_ADAPTER_H_
