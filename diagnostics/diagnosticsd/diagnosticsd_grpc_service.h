// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_DIAGNOSTICSD_DIAGNOSTICSD_GRPC_SERVICE_H_
#define DIAGNOSTICS_DIAGNOSTICSD_DIAGNOSTICSD_GRPC_SERVICE_H_

#include <memory>

#include <base/callback.h>
#include <base/macros.h>

#include "common.pb.h"        // NOLINT(build/include)
#include "diagnosticsd.pb.h"  // NOLINT(build/include)

namespace diagnostics {

// Implements the "Diagnosticsd" gRPC interface exposed by the diagnosticsd
// daemon (see the API definition at grpc/diagnosticsd.proto).
class DiagnosticsdGrpcService final {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
  };

  using SendMessageToUiCallback =
      base::Callback<void(std::unique_ptr<grpc_api::EmptyMessage>)>;

  explicit DiagnosticsdGrpcService(Delegate* delegate);
  ~DiagnosticsdGrpcService();

  // Implementation of the "Diagnosticsd" gRPC interface:
  void SendMessageToUi(
      std::unique_ptr<grpc_api::SendMessageToUiRequest> request,
      const SendMessageToUiCallback& callback);

 private:
  // Unowned. The delegate should outlive this instance.
  Delegate* const delegate_;

  DISALLOW_COPY_AND_ASSIGN(DiagnosticsdGrpcService);
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_DIAGNOSTICSD_DIAGNOSTICSD_GRPC_SERVICE_H_
