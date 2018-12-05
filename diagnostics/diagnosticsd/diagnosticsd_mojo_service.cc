// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/diagnosticsd/diagnosticsd_mojo_service.h"

#include <memory>
#include <utility>

#include <base/json/json_reader.h>
#include <base/logging.h>
#include <base/memory/shared_memory.h>
#include <base/values.h>

#include "diagnostics/diagnosticsd/mojo_utils.h"

namespace diagnostics {

using SendUiMessageToDiagnosticsProcessorCallback =
    DiagnosticsdMojoService::SendUiMessageToDiagnosticsProcessorCallback;

namespace {

void ForwardMojoJsonResponse(
    const SendUiMessageToDiagnosticsProcessorCallback& mojo_response_callback,
    std::string response_json_message) {
  // TODO(lamzin@google.com): Forward the response message contents.
  mojo_response_callback.Run(mojo::ScopedHandle() /* response_json_message */);
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

  int json_error_code = base::JSONReader::JSON_NO_ERROR;
  int json_error_line, json_error_column;
  std::string json_error_message;
  base::JSONReader::ReadAndReturnError(
      json_message_content, base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS,
      &json_error_code, &json_error_message, &json_error_line,
      &json_error_column);
  if (json_error_code == base::JSONReader::JSON_NO_ERROR) {
    delegate_->SendGrpcUiMessageToDiagnosticsProcessor(
        json_message_content, base::Bind(&ForwardMojoJsonResponse, callback));
  } else {
    LOG(ERROR) << "Invalid JSON at line " << json_error_line << " and column "
               << json_error_column
               << ". JSON parsing message: " << json_error_message
               << ". Error code: " << json_error_code;
    callback.Run(mojo::ScopedHandle() /* response_json_message */);
  }
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
