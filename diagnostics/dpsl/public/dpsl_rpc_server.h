// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_DPSL_PUBLIC_DPSL_RPC_SERVER_H_
#define DIAGNOSTICS_DPSL_PUBLIC_DPSL_RPC_SERVER_H_

#include <memory>

#include "diagnostics/dpsl/public/export.h"

namespace diagnostics {

class DpslRpcHandler;
class DpslThreadContext;

// Interface of the class that runs a gRPC server listening on the specified
// URI. All incoming gRPC requests are passed to the given DpslRpcHandler
// instance.
//
// Obtain an instance of this class via the Create() method. For hints on usage,
// see dpsl_rpc_handler.h.
//
// NOTE ON THREADING MODEL: This class is NOT thread-safe. This instance must be
// destroyed on the same thread on which it was created. The DPSL itself
// guarantees that it will run methods of the given DpslRpcHandler instance on
// that same thread too.
//
// PRECONDITIONS:
// 1. An instance of DpslThreadContext must exist on the current thread during
//    the whole lifetime of this object.
class DPSL_EXPORT DpslRpcServer {
 public:
  // Specifies predefined options for the URI on which the started gRPC server
  // should be listening.
  enum class GrpcServerUri {
    // A Unix domain socket at the predefined constant path. This option is
    // available only when running OUTSIDE a VM.
    // Only one server with this URI may run at a time; breaking this
    // requirement will lead to unspecified behavior.
    kLocalDomainSocket = 0,
    // A Unix domain socket at the predefined constant path. This option is
    // available only when running OUTSIDE a VM. A server is eligible to
    // receive EC notifications and messages from UI extension (hosted by
    // browser). No other server is eligible to receive UI messages.
    // Only one server with this URI may run at a time; breaking this
    // requirement will lead to unspecified behavior.
    kUiMessageReceiverDomainSocket = 1,
  };

  // Factory method that returns an instance of the real implementation of this
  // interface.
  //
  // Returns a null pointer when the server startup fails (for example, when the
  // specified gRPC URI is unavailable).
  //
  // Both |thread_context| and |rpc_handler| are passed as unowned pointers;
  // they must outlive the created DpslRpcServer instance.
  static std::unique_ptr<DpslRpcServer> Create(
      DpslThreadContext* thread_context,
      DpslRpcHandler* rpc_handler,
      GrpcServerUri grpc_server_uri);

  virtual ~DpslRpcServer() = default;
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_DPSL_PUBLIC_DPSL_RPC_SERVER_H_
