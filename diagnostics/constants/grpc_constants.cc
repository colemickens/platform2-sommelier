// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/constants/grpc_constants.h"

namespace diagnostics {

// URI on which the gRPC interface exposed by the wilco_dtc_supportd daemon is
// listening.
const char kWilcoDtcSupportdGrpcUri[] =
    "unix:/run/wilco_dtc/grpc_sockets/wilco_dtc_supportd_socket";

// URI which is used for making requests to the gRPC interface exposed by the
// wilco_dtc daemons. Not eligible to receive UI messages.
const char kWilcoDtcGrpcUri[] =
    "unix:/run/wilco_dtc/grpc_sockets/wilco_dtc_socket";

// URI which is used for making requests to the gRPC interface exposed by the
// wilco_dtc daemons. Eligible to receive UI messages.
const char kUiMessageReceiverWilcoDtcGrpcUri[] =
    "unix:/run/wilco_dtc/grpc_sockets/ui_message_receiver_socket";

}  // namespace diagnostics
