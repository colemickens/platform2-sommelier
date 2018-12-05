// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_DIAGNOSTICSD_MOCK_MOJOM_DIAGNOSTICSD_CLIENT_H_
#define DIAGNOSTICS_DIAGNOSTICSD_MOCK_MOJOM_DIAGNOSTICSD_CLIENT_H_

#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <mojo/public/cpp/system/buffer.h>

#include "mojo/diagnosticsd.mojom.h"

namespace diagnostics {

class MockMojomDiagnosticsdClient
    : public chromeos::diagnosticsd::mojom::DiagnosticsdClient {
 public:
  using MojoDiagnosticsdWebRequestHttpMethod =
      chromeos::diagnosticsd::mojom::DiagnosticsdWebRequestHttpMethod;
  using MojoDiagnosticsdWebRequestStatus =
      chromeos::diagnosticsd::mojom::DiagnosticsdWebRequestStatus;
  using MojoPerformWebRequestCallback =
      base::Callback<void(MojoDiagnosticsdWebRequestStatus, int)>;

  void SendDiagnosticsProcessorMessageToUi(
      mojo::ScopedHandle json_message,
      const SendDiagnosticsProcessorMessageToUiCallback& callback) override {
    // Redirect to a separate mockable method to workaround GMock's issues with
    // move-only parameters.
    SendDiagnosticsProcessorMessageToUiImpl(&json_message, callback);
  }

  // TODO(pbond): mock this method.
  void PerformWebRequest(
      MojoDiagnosticsdWebRequestHttpMethod http_method,
      const std::string& url,
      const std::vector<std::string>& headers,
      const std::string& request_body,
      const MojoPerformWebRequestCallback& callback) override {
    PerformWebRequestImpl(http_method, url, headers, request_body, callback);
    // The callback must be called.
    callback.Run(MojoDiagnosticsdWebRequestStatus::kOk, 200 /* HTTP status */);
  }

  MOCK_METHOD2(
      SendDiagnosticsProcessorMessageToUiImpl,
      void(mojo::ScopedHandle* json_message,
           const SendDiagnosticsProcessorMessageToUiCallback& callback));
  MOCK_METHOD5(PerformWebRequestImpl,
               void(MojoDiagnosticsdWebRequestHttpMethod http_method,
                    const std::string& url,
                    const std::vector<std::string>& headers,
                    const std::string& request_body,
                    const MojoPerformWebRequestCallback& callback));
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_DIAGNOSTICSD_MOCK_MOJOM_DIAGNOSTICSD_CLIENT_H_
