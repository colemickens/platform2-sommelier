// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_DIAGNOSTICSD_MOCK_MOJOM_DIAGNOSTICSD_CLIENT_H_
#define DIAGNOSTICS_DIAGNOSTICSD_MOCK_MOJOM_DIAGNOSTICSD_CLIENT_H_

#include "mojo/diagnosticsd.mojom.h"

namespace diagnostics {

class MockMojomDiagnosticsdClient
    : public chromeos::diagnosticsd::mojom::DiagnosticsdClient {};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_DIAGNOSTICSD_MOCK_MOJOM_DIAGNOSTICSD_CLIENT_H_
