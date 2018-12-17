// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/diagnosticsd/diagnosticsd_mojo_service.h"

#include <memory>
#include <utility>

#include <base/logging.h>
#include <base/memory/shared_memory.h>

#include "diagnostics/diagnosticsd/json_utils.h"
#include "diagnostics/diagnosticsd/mojo_utils.h"

namespace diagnostics {

using SendUiMessageToDiagnosticsProcessorCallback =
    DiagnosticsdMojoService::SendUiMessageToDiagnosticsProcessorCallback;

namespace {

void ForwardMojoJsonResponse(
    const SendUiMessageToDiagnosticsProcessorCallback& mojo_response_callback,
    std::string response_json_message) {
  if (response_json_message.empty()) {
    mojo_response_callback.Run(
        mojo::ScopedHandle() /* response_json_message */);
    return;
  }
  mojo::ScopedHandle response_json_message_handle =
      CreateReadOnlySharedMemoryMojoHandle(
          base::StringPiece(response_json_message));
  mojo_response_callback.Run(std::move(response_json_message_handle));
}

}  // namespace

DiagnosticsdMojoService::DiagnosticsdMojoService(
    Delegate* delegate,
    MojomDiagnosticsdServiceRequest self_interface_request,
    MojomDiagnosticsdClientPtr client_ptr)
    : delegate_(delegate),
      self_binding_(this /* impl */, std::move(self_interface_request)),
      client_ptr_(std::move(client_ptr)) {
  DCHECK(delegate_);
  DCHECK(self_binding_.is_bound());
  DCHECK(client_ptr_);
}

DiagnosticsdMojoService::~DiagnosticsdMojoService() = default;

void DiagnosticsdMojoService::SendUiMessageToDiagnosticsProcessor(
    mojo::ScopedHandle json_message,
    const SendUiMessageToDiagnosticsProcessorCallback& callback) {
  std::unique_ptr<base::SharedMemory> shared_memory =
      GetReadOnlySharedMemoryFromMojoHandle(std::move(json_message));
  if (!shared_memory) {
    LOG(ERROR) << "Failed to read data from mojo handle";
    callback.Run(mojo::ScopedHandle() /* response_json_message */);
    return;
  }
  base::StringPiece json_message_content(
      static_cast<const char*>(shared_memory->memory()),
      shared_memory->mapped_size());

  std::string json_error_message;
  if (!IsJsonValid(json_message_content, &json_error_message)) {
    LOG(ERROR) << "Invalid JSON error: " << json_error_message;
    callback.Run(mojo::ScopedHandle() /* response_json_message */);
    return;
  }

  delegate_->SendGrpcUiMessageToDiagnosticsProcessor(
      json_message_content, base::Bind(&ForwardMojoJsonResponse, callback));
}

void DiagnosticsdMojoService::PerformWebRequest(
    MojomDiagnosticsdWebRequestHttpMethod http_method,
    const std::string& url,
    const std::vector<std::string>& headers,
    const std::string& request_body,
    const MojomPerformWebRequestCallback& callback) {
  DCHECK(client_ptr_);
  client_ptr_->PerformWebRequest(http_method, url, headers, request_body,
                                 callback);
}

}  // namespace diagnostics
