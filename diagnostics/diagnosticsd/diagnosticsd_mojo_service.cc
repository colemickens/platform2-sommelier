// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/diagnosticsd/diagnosticsd_mojo_service.h"

#include <utility>

#include <base/logging.h>

namespace diagnostics {

DiagnosticsdMojoService::DiagnosticsdMojoService(Delegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
}

DiagnosticsdMojoService::~DiagnosticsdMojoService() = default;

void DiagnosticsdMojoService::Init(MojomDiagnosticsdServiceClientPtr client_ptr,
                                   const InitCallback& callback) {
  VLOG(0) << "Received Init Mojo request";
  if (!client_ptr)
    LOG(ERROR) << "Invalid Mojo client interface pointer passed to Init";
  else
    client_ptr_ = std::move(client_ptr);
  callback.Run();
}

void DiagnosticsdMojoService::SendUiMessageToDiagnosticsProcessor(
    mojo::ScopedSharedBufferHandle json_message,
    const SendUiMessageToDiagnosticsProcessorCallback& callback) {
  NOTIMPLEMENTED();
}

}  // namespace diagnostics
