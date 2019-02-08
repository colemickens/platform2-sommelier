// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/constants/grpc_constants.h"

namespace diagnostics {

// URI on which the gRPC interface exposed by the diagnosticsd daemon is
// listening.
const char kDiagnosticsdGrpcUri[] =
    "unix:/run/diagnostics/grpc_sockets/diagnosticsd_socket";

// URI which is used for making requests to the gRPC interface exposed by the
// diagnostics_processor daemons. Not eligible to receive UI messages.
const char kDiagnosticsProcessorGrpcUri[] =
    "unix:/run/diagnostics/grpc_sockets/diagnostics_processor_socket";

// URI which is used for making requests to the gRPC interface exposed by the
// diagnostics_processor daemons. Eligible to receive UI messages.
const char kUiMessageReceiverDiagnosticsProcessorGrpcUri[] =
    "unix:/run/diagnostics/grpc_sockets/ui_message_receiver_socket";

}  // namespace diagnostics
