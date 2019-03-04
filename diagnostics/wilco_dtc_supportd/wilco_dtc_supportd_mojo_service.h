// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_WILCO_DTC_SUPPORTD_WILCO_DTC_SUPPORTD_MOJO_SERVICE_H_
#define DIAGNOSTICS_WILCO_DTC_SUPPORTD_WILCO_DTC_SUPPORTD_MOJO_SERVICE_H_

#include <string>
#include <vector>

#include <base/callback.h>
#include <base/macros.h>
#include <base/optional.h>
#include <base/strings/string_piece.h>
#include <mojo/public/cpp/bindings/binding.h>
#include <mojo/public/cpp/system/buffer.h>

#include "mojo/diagnosticsd.mojom.h"

namespace diagnostics {

// Implements the "DiagnosticsdService" Mojo interface exposed by the
// wilco_dtc_supportd daemon (see the API definition at mojo/diagnosticsd.mojom)
class WilcoDtcSupportdMojoService final
    : public chromeos::diagnosticsd::mojom::DiagnosticsdService {
 public:
  using MojomDiagnosticsdClientPtr =
      chromeos::diagnosticsd::mojom::DiagnosticsdClientPtr;
  using MojomDiagnosticsdService =
      chromeos::diagnosticsd::mojom::DiagnosticsdService;
  using MojomDiagnosticsdServiceRequest =
      chromeos::diagnosticsd::mojom::DiagnosticsdServiceRequest;
  using MojomDiagnosticsdWebRequestHttpMethod =
      chromeos::diagnosticsd::mojom::DiagnosticsdWebRequestHttpMethod;
  using MojomDiagnosticsdWebRequestStatus =
      chromeos::diagnosticsd::mojom::DiagnosticsdWebRequestStatus;
  using MojomPerformWebRequestCallback = base::Callback<void(
      MojomDiagnosticsdWebRequestStatus, int, base::StringPiece)>;

  class Delegate {
   public:
    using SendGrpcUiMessageToWilcoDtcCallback =
        base::Callback<void(std::string response_json_message)>;

    virtual ~Delegate() = default;

    // Called when wilco_dtc_supportd daemon mojo function
    // |SendUiMessageToDiagnosticsProcessor| was called.
    //
    // Calls gRPC HandleMessageFromUiRequest method on wilco_dtc and puts
    // |json_message| to the gRPC |HandleMessageFromUiRequest| request message.
    // Result of the call is returned via |callback|; if the request succeeded,
    // it will receive the message returned by the wilco_dtc.
    virtual void SendGrpcUiMessageToWilcoDtc(
        base::StringPiece json_message,
        const SendGrpcUiMessageToWilcoDtcCallback& callback) = 0;
  };

  // |delegate| - Unowned pointer; must outlive this instance.
  // |self_interface_request| - Mojo interface request that will be fulfilled
  // by this instance. In production, this interface request is created by the
  // browser process, and allows the browser to call our methods.
  // |client_ptr| - Mojo interface to the DiagnosticsdServiceClient endpoint. In
  // production, it allows this instance to call browser's methods.
  WilcoDtcSupportdMojoService(
      Delegate* delegate,
      MojomDiagnosticsdServiceRequest self_interface_request,
      MojomDiagnosticsdClientPtr client_ptr);
  ~WilcoDtcSupportdMojoService() override;

  // chromeos::diagnosticsd::mojom::DiagnosticsdService overrides:
  void SendUiMessageToDiagnosticsProcessor(
      mojo::ScopedHandle json_message,
      const SendUiMessageToDiagnosticsProcessorCallback& callback) override;

  // Calls to DiagnosticsdClient.
  void PerformWebRequest(MojomDiagnosticsdWebRequestHttpMethod http_method,
                         const std::string& url,
                         const std::vector<std::string>& headers,
                         const std::string& request_body,
                         const MojomPerformWebRequestCallback& callback);

 private:
  // Unowned. The delegate should outlive this instance.
  Delegate* const delegate_;

  // Mojo binding that connects |this| with the message pipe, allowing the
  // remote end to call our methods.
  const mojo::Binding<MojomDiagnosticsdService> self_binding_;

  // Mojo interface to the DiagnosticsdServiceClient endpoint.
  //
  // In production this interface is implemented in the Chrome browser process.
  MojomDiagnosticsdClientPtr client_ptr_;

  DISALLOW_COPY_AND_ASSIGN(WilcoDtcSupportdMojoService);
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_WILCO_DTC_SUPPORTD_WILCO_DTC_SUPPORTD_MOJO_SERVICE_H_
