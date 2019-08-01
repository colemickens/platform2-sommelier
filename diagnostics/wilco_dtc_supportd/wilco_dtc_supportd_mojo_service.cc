// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/wilco_dtc_supportd/wilco_dtc_supportd_mojo_service.h"

#include <memory>
#include <utility>

#include <base/logging.h>
#include <base/memory/shared_memory.h>

#include "diagnostics/wilco_dtc_supportd/json_utils.h"
#include "diagnostics/wilco_dtc_supportd/mojo_utils.h"

namespace diagnostics {

using SendUiMessageToWilcoDtcCallback =
    WilcoDtcSupportdMojoService::SendUiMessageToWilcoDtcCallback;

namespace {

void ForwardMojoJsonResponse(
    const SendUiMessageToWilcoDtcCallback& mojo_response_callback,
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

void ForwardMojoWebResponse(
    const WilcoDtcSupportdMojoService::MojomPerformWebRequestCallback& callback,
    WilcoDtcSupportdMojoService::MojomWilcoDtcSupportdWebRequestStatus status,
    int http_status,
    mojo::ScopedHandle response_body_handle) {
  if (!response_body_handle.is_valid()) {
    callback.Run(status, http_status, base::StringPiece());
    return;
  }
  auto shared_memory =
      GetReadOnlySharedMemoryFromMojoHandle(std::move(response_body_handle));
  if (!shared_memory) {
    LOG(ERROR) << "Failed to read data from mojo handle";
    callback.Run(WilcoDtcSupportdMojoService::
                     MojomWilcoDtcSupportdWebRequestStatus::kNetworkError,
                 0, base::StringPiece());
    return;
  }
  callback.Run(
      status, http_status,
      base::StringPiece(static_cast<const char*>(shared_memory->memory()),
                        shared_memory->mapped_size()));
}

}  // namespace

WilcoDtcSupportdMojoService::WilcoDtcSupportdMojoService(
    Delegate* delegate,
    MojomWilcoDtcSupportdServiceRequest self_interface_request,
    MojomWilcoDtcSupportdClientPtr client_ptr)
    : delegate_(delegate),
      self_binding_(this /* impl */, std::move(self_interface_request)),
      client_ptr_(std::move(client_ptr)) {
  DCHECK(delegate_);
  DCHECK(self_binding_.is_bound());
  DCHECK(client_ptr_);
}

WilcoDtcSupportdMojoService::~WilcoDtcSupportdMojoService() = default;

void WilcoDtcSupportdMojoService::SendUiMessageToWilcoDtc(
    mojo::ScopedHandle json_message,
    const SendUiMessageToWilcoDtcCallback& callback) {
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

  delegate_->SendGrpcUiMessageToWilcoDtc(
      json_message_content, base::Bind(&ForwardMojoJsonResponse, callback));
}

void WilcoDtcSupportdMojoService::NotifyConfigurationDataChanged() {
  delegate_->NotifyConfigurationDataChangedToWilcoDtc();
}

void WilcoDtcSupportdMojoService::PerformWebRequest(
    MojomWilcoDtcSupportdWebRequestHttpMethod http_method,
    const std::string& url,
    const std::vector<std::string>& headers,
    const std::string& request_body,
    const MojomPerformWebRequestCallback& callback) {
  DCHECK(client_ptr_);
  mojo::ScopedHandle url_handle = CreateReadOnlySharedMemoryMojoHandle(url);
  if (!url_handle.is_valid()) {
    LOG(ERROR) << "Failed to create a mojo handle.";
    callback.Run(MojomWilcoDtcSupportdWebRequestStatus::kNetworkError, 0,
                 base::StringPiece());
    return;
  }

  std::vector<mojo::ScopedHandle> header_handles;
  for (const auto& header : headers) {
    header_handles.push_back(CreateReadOnlySharedMemoryMojoHandle(header));
    if (!header_handles.back().is_valid()) {
      LOG(ERROR) << "Failed to create a mojo handle.";
      callback.Run(MojomWilcoDtcSupportdWebRequestStatus::kNetworkError, 0,
                   base::StringPiece());
      return;
    }
  }
  mojo::ScopedHandle request_body_handle =
      CreateReadOnlySharedMemoryMojoHandle(request_body);
  // Invalid handle for an empty |request_body| does not cause an error.
  if (!request_body.empty() && !request_body_handle.is_valid()) {
    LOG(ERROR) << "Failed to create a mojo handle.";
    callback.Run(MojomWilcoDtcSupportdWebRequestStatus::kNetworkError, 0,
                 base::StringPiece());
    return;
  }

  client_ptr_->PerformWebRequest(http_method, std::move(url_handle),
                                 std::move(header_handles),
                                 std::move(request_body_handle),
                                 base::Bind(&ForwardMojoWebResponse, callback));
}

void WilcoDtcSupportdMojoService::GetConfigurationData(
    const MojomGetConfigurationDataCallback& callback) {
  DCHECK(client_ptr_);
  client_ptr_->GetConfigurationData(callback);
}

void WilcoDtcSupportdMojoService::HandleEvent(
    const MojomWilcoDtcSupportdEvent event) {
  client_ptr_->HandleEvent(event);
}
}  // namespace diagnostics
