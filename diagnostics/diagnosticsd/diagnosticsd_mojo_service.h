// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_DIAGNOSTICSD_DIAGNOSTICSD_MOJO_SERVICE_H_
#define DIAGNOSTICS_DIAGNOSTICSD_DIAGNOSTICSD_MOJO_SERVICE_H_

#include <base/callback.h>
#include <base/macros.h>
#include <mojo/public/cpp/system/buffer.h>

#include "mojo/diagnosticsd.mojom.h"

namespace diagnostics {

// Implements the "DiagnosticsdService" Mojo interface exposed by the
// diagnosticsd daemon (see the API definition at mojo/diagnosticsd.mojom).
class DiagnosticsdMojoService final
    : public chromeos::diagnostics::mojom::DiagnosticsdService {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
  };

  explicit DiagnosticsdMojoService(Delegate* delegate);
  ~DiagnosticsdMojoService();

  // chromeos::diagnostics::mojom::DiagnosticsdService overrides:
  void Init(
      chromeos::diagnostics::mojom::DiagnosticsdServiceClientPtr client_ptr,
      const InitCallback& callback) override;
  void SendUiMessageToDiagnosticsProcessor(
      mojo::ScopedSharedBufferHandle json_message,
      const SendUiMessageToDiagnosticsProcessorCallback& callback) override;

 private:
  // Unowned. The delegate should outlive this instance.
  Delegate* const delegate_;

  DISALLOW_COPY_AND_ASSIGN(DiagnosticsdMojoService);
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_DIAGNOSTICSD_DIAGNOSTICSD_MOJO_SERVICE_H_
