// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_DIAGNOSTICSD_DIAGNOSTICSD_MOJO_SERVICE_H_
#define DIAGNOSTICS_DIAGNOSTICSD_DIAGNOSTICSD_MOJO_SERVICE_H_

#include <base/callback.h>
#include <base/macros.h>
#include <base/strings/string_piece.h>
#include <mojo/public/cpp/system/buffer.h>

#include "mojo/diagnosticsd.mojom.h"

namespace diagnostics {

// Implements the "DiagnosticsdService" Mojo interface exposed by the
// diagnosticsd daemon (see the API definition at mojo/diagnosticsd.mojom).
class DiagnosticsdMojoService final
    : public chromeos::diagnostics::mojom::DiagnosticsdService {
 public:
  using MojomDiagnosticsdServiceClientPtr =
      chromeos::diagnostics::mojom::DiagnosticsdServiceClientPtr;

  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Called when diagnosticsd daemon mojo function
    // |SendUiMessageToDiagnosticsProcessorWithSize| was called.
    //
    // Call gRPC HandleMessageFromUiRequest method on diagnostics processor
    // and put |json_message| to the gRPC
    // |HandleMessageFromUiRequest| request message.
    virtual void SendGrpcUiMessageToDiagnosticsProcessor(
        base::StringPiece json_message) = 0;
  };

  explicit DiagnosticsdMojoService(Delegate* delegate);
  ~DiagnosticsdMojoService();

  // chromeos::diagnostics::mojom::DiagnosticsdService overrides:
  void Init(MojomDiagnosticsdServiceClientPtr client_ptr,
            const InitCallback& callback) override;
  void SendUiMessageToDiagnosticsProcessor(
      mojo::ScopedSharedBufferHandle json_message,
      const SendUiMessageToDiagnosticsProcessorCallback& callback) override;
  void SendUiMessageToDiagnosticsProcessorWithSize(
      mojo::ScopedSharedBufferHandle json_message,
      int64_t json_message_size,
      const SendUiMessageToDiagnosticsProcessorWithSizeCallback& callback)
      override;

 private:
  // Unowned. The delegate should outlive this instance.
  Delegate* const delegate_;

  // Interface to the DiagnosticsdServiceClient endpoint. This field becomes
  // initialized after the Init() call.
  //
  // In production this interface is implemented in the Chrome browser process.
  MojomDiagnosticsdServiceClientPtr client_ptr_;

  DISALLOW_COPY_AND_ASSIGN(DiagnosticsdMojoService);
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_DIAGNOSTICSD_DIAGNOSTICSD_MOJO_SERVICE_H_
