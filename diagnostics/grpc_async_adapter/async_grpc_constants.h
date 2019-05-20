// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_GRPC_ASYNC_ADAPTER_ASYNC_GRPC_CONSTANTS_H_
#define DIAGNOSTICS_GRPC_ASYNC_ADAPTER_ASYNC_GRPC_CONSTANTS_H_

namespace diagnostics {

// Use this constant to explicitly set gRPC max send/receive message lengths,
// because GRPC_DEFAULT_MAX_SEND_MESSAGE_LENGTH const is -1.
// GRPC_DEFAULT_MAX_SEND_MESSAGE_LENGTH will be used as a default value if max
// send message length is not configured for client and server.
extern const int kMaxGrpcMessageSize;

}  // namespace diagnostics

#endif  // DIAGNOSTICS_GRPC_ASYNC_ADAPTER_ASYNC_GRPC_CONSTANTS_H_
