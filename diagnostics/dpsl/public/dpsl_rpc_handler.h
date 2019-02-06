// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_DPSL_PUBLIC_DPSL_RPC_HANDLER_H_
#define DIAGNOSTICS_DPSL_PUBLIC_DPSL_RPC_HANDLER_H_

#include <functional>
#include <memory>

#include "diagnostics_processor.pb.h"  // NOLINT(build/include)

namespace diagnostics {

// Abstract class that corresponds to the DiagnosticsProcessor gRPC interface
// (defined at grpc/diagnostics_processor.proto).
//
// EXAMPLE USAGE:
//
//   class MyRpcHandler : public DpslRpcHandler {
//     void HandleMessageFromUi(...) override {
//       ... // custom logic
//     }
//   };
//
//   MyRpcHandler my_rpc_handler;
//   auto thread_context = DpslThreadContext.Create(...);
//   auto rpc_server = DpslRpcServer::Create(..., &my_rpc_handler, ...);
//   thread_context->RunEventLoop();
//
// This will start a gRPC server that listens for incoming requests on the
// DiagnosticsProcessor interface at the specified gRPC URI. These requests will
// be transformed by DPSL into |my_rpc_handler|'s method calls.
//
// NOTE ON THREADING MODEL: The DPSL implementation ensures that, whenever it
// calls methods of this class, it does that on the same thread - the one on
// which DpslRpcServer was created.
// It's allowed to call the callbacks, which are supplied by DPSL to methods
// of this class, from any thread.
//
// NOTE ON REQUESTS SEQUENCE: Parallel requests are possible: DPSL may make a
// new call of a method of this class before the previous one ran its callback.
//
// NOTE ON LONG-RUNNING TASKS: It's recommended to avoid doing long-running
// tasks in overridden methods of this class on the current thread, since this
// would block the thread from running other jobs, like serving subsequent
// incoming requests. Therefore it's advisable to offload time-consuming
// operations onto background threads.
class DPSL_EXPORT DpslRpcHandler {
 public:
  // Request-specific callback types. These callbacks are passed by DPSL and
  // should be used by the implementation to return method results.
  //
  // When |response| is passed as null, the whole request is considered canceled
  // (i.e., the diagnosticsd daemon receives the cancellation error for this
  // request).
  using HandleMessageFromUiCallback = std::function<void(
      std::unique_ptr<grpc_api::HandleMessageFromUiResponse> response)>;
  using HandleEcNotificationCallback = std::function<void(
      std::unique_ptr<grpc_api::HandleEcNotificationResponse> response)>;

  virtual ~DpslRpcHandler() = default;

  // Methods of the DiagnosticsProcessor gRPC interface.
  //
  // The |request| parameters are guaranteed to be non-null. The supplied
  // |callback| must be run no more than once (and until this happens, the
  // request is considered as running and consumes resources). It's allowed to
  // run |callback| from any thread.

  virtual void HandleMessageFromUi(
      std::unique_ptr<grpc_api::HandleMessageFromUiRequest> request,
      HandleMessageFromUiCallback callback) = 0;
  virtual void HandleEcNotification(
      std::unique_ptr<grpc_api::HandleEcNotificationRequest> request,
      HandleEcNotificationCallback callback) = 0;
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_DPSL_PUBLIC_DPSL_RPC_HANDLER_H_
