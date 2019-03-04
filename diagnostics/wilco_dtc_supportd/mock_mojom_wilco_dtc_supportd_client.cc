// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/wilco_dtc_supportd/mock_mojom_wilco_dtc_supportd_client.h"

#include <utility>

#include "diagnostics/wilco_dtc_supportd/mojo_test_utils.h"
#include "diagnostics/wilco_dtc_supportd/mojo_utils.h"

namespace diagnostics {

void MockMojomWilcoDtcSupportdClient::SendDiagnosticsProcessorMessageToUi(
    mojo::ScopedHandle json_message,
    const SendDiagnosticsProcessorMessageToUiCallback& callback) {
  // Redirect to a separate mockable method to workaround GMock's issues with
  // move-only parameters.
  SendDiagnosticsProcessorMessageToUiImpl(&json_message, callback);
}

void MockMojomWilcoDtcSupportdClient::PerformWebRequest(
    MojoDiagnosticsdWebRequestHttpMethod http_method,
    mojo::ScopedHandle url,
    std::vector<mojo::ScopedHandle> headers,
    mojo::ScopedHandle request_body,
    const MojoPerformWebRequestCallback& callback) {
  // Extract string content from mojo::Handle's.
  std::string url_content = GetStringFromMojoHandle(std::move(url));
  std::vector<std::string> header_contents;
  for (auto& header : headers) {
    header_contents.push_back(GetStringFromMojoHandle(std::move(header)));
  }
  std::string request_body_content =
      GetStringFromMojoHandle(std::move(request_body));

  // Redirect to a separate mockable method to workaround GMock's issues with
  // move-only parameters.
  PerformWebRequestImpl(http_method, url_content, header_contents,
                        request_body_content);
  // The callback must be called.
  callback.Run(MojoDiagnosticsdWebRequestStatus::kOk, 200 /* HTTP status */,
               CreateReadOnlySharedMemoryMojoHandle(request_body_content));
}

}  // namespace diagnostics
