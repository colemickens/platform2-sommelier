// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/diagnosticsd/diagnosticsd_grpc_service.h"

#include <base/logging.h>

namespace diagnostics {

DiagnosticsdGrpcService::DiagnosticsdGrpcService(Delegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
}

DiagnosticsdGrpcService::~DiagnosticsdGrpcService() = default;

void DiagnosticsdGrpcService::SendMessageToUi(
    std::unique_ptr<grpc_api::SendMessageToUiRequest> request,
    const SendMessageToUiCallback& callback) {
  NOTIMPLEMENTED();
}

}  // namespace diagnostics
