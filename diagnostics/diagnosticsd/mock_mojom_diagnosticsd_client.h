// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_DIAGNOSTICSD_MOCK_MOJOM_DIAGNOSTICSD_CLIENT_H_
#define DIAGNOSTICS_DIAGNOSTICSD_MOCK_MOJOM_DIAGNOSTICSD_CLIENT_H_

#include <gtest/gtest.h>
#include <mojo/public/cpp/system/buffer.h>

#include "mojo/diagnosticsd.mojom.h"

namespace diagnostics {

class MockMojomDiagnosticsdClient
    : public chromeos::diagnosticsd::mojom::DiagnosticsdClient {
 public:
  void SendDiagnosticsProcessorMessageToUi(
      mojo::ScopedSharedBufferHandle json_message,
      const SendDiagnosticsProcessorMessageToUiCallback& callback) override {
    // Redirect to a separate mockable method to workaround GMock's issues with
    // move-only parameters.
    SendDiagnosticsProcessorMessageToUiImpl(&json_message, callback);
  }

  MOCK_METHOD2(
      SendDiagnosticsProcessorMessageToUiImpl,
      void(mojo::ScopedSharedBufferHandle* json_message,
           const SendDiagnosticsProcessorMessageToUiCallback& callback));
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_DIAGNOSTICSD_MOCK_MOJOM_DIAGNOSTICSD_CLIENT_H_
