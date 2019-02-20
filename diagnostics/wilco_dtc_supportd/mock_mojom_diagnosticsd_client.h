// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_WILCO_DTC_SUPPORTD_MOCK_MOJOM_DIAGNOSTICSD_CLIENT_H_
#define DIAGNOSTICS_WILCO_DTC_SUPPORTD_MOCK_MOJOM_DIAGNOSTICSD_CLIENT_H_

#include <string>
#include <vector>

#include <gmock/gmock.h>
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
  using MojoPerformWebRequestCallback = base::Callback<void(
      MojoDiagnosticsdWebRequestStatus, int, mojo::ScopedHandle)>;

  void SendDiagnosticsProcessorMessageToUi(
      mojo::ScopedHandle json_message,
      const SendDiagnosticsProcessorMessageToUiCallback& callback) override;

  void PerformWebRequest(
      MojoDiagnosticsdWebRequestHttpMethod http_method,
      mojo::ScopedHandle url,
      std::vector<mojo::ScopedHandle> headers,
      mojo::ScopedHandle request_body,
      const MojoPerformWebRequestCallback& callback) override;

  MOCK_METHOD2(
      SendDiagnosticsProcessorMessageToUiImpl,
      void(mojo::ScopedHandle* json_message,
           const SendDiagnosticsProcessorMessageToUiCallback& callback));
  MOCK_METHOD4(PerformWebRequestImpl,
               void(MojoDiagnosticsdWebRequestHttpMethod http_method,
                    const std::string& url,
                    const std::vector<std::string>& headers,
                    const std::string& request_body));
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_WILCO_DTC_SUPPORTD_MOCK_MOJOM_DIAGNOSTICSD_CLIENT_H_
