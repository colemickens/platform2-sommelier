// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/bind.h>
#include <base/callback.h>
#include <base/logging.h>
#include <base/macros.h>
#include <base/memory/weak_ptr.h>
#include <base/message_loop/message_loop.h>
#include <base/run_loop.h>
#include <brillo/flag_helper.h>

#include "diagnostics/grpc_async_adapter/async_grpc_client.h"
#include "diagnosticsd.grpc.pb.h"  // NOLINT(build/include)
#include "diagnosticsd.pb.h"       // NOLINT(build/include)

namespace {
// A utility for testing outgoing RPCs. It gets notified of a response to an
// outgoing RPC through the callback it returns from |MakeWriter|.
template <typename ResponseType>
class RpcReply {
 public:
  RpcReply() : weak_ptr_factory_(this) {}
  ~RpcReply() = default;

  // Returns a callback that is called when a response to the outgoing
  // RPC is available.
  base::Callback<void(std::unique_ptr<ResponseType>)> MakeWriter() {
    return base::Bind(&RpcReply::OnReply, weak_ptr_factory_.GetWeakPtr());
  }

  // Wait until this RPC has a reply.
  void Wait() {
    if (has_reply_)
      return;

    waiting_loop_ = std::make_unique<base::RunLoop>();
    waiting_loop_->Run();
  }

  // Returns true if the reply indicated an error. This may only be called after
  // |Wait| returned.
  bool IsError() const {
    CHECK(has_reply_);
    return response_ == nullptr;
  }

  // Returns this outgoing RPC's response. This may only be called after
  // |Wait| returned and when |IsError| is false.
  const ResponseType& response() const {
    CHECK(!IsError());
    return *response_;
  }

 private:
  void OnReply(std::unique_ptr<ResponseType> response) {
    CHECK(!has_reply_);

    has_reply_ = true;
    response_ = std::move(response);

    if (waiting_loop_)
      waiting_loop_->Quit();
  }

  std::unique_ptr<base::RunLoop> waiting_loop_;
  bool has_reply_ = false;
  std::unique_ptr<ResponseType> response_;

  base::WeakPtrFactory<RpcReply> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RpcReply);
};

// URI on which the gRPC interface exposed by the diagnosticsd daemon is
// listening.
constexpr char kDiagnosticsdGrpcUri[] =
    "unix:/run/diagnostics/grpc_sockets/diagnosticsd_socket";
}  // namespace

int main(int argc, char** argv) {
  brillo::FlagHelper::Init(argc, argv, "telem - Device telemetry tool.");

  logging::InitLogging(logging::LoggingSettings());

  base::MessageLoopForIO message_loop_;
  // Allows making outgoing requests to the gRPC interface exposed by the
  // diagnosticsd daemon.
  std::unique_ptr<
      diagnostics::AsyncGrpcClient<diagnostics::grpc_api::Diagnosticsd>>
      diagnosticsd_grpc_client_;

  // Create the AsyncGrpcClient.
  diagnosticsd_grpc_client_ = std::make_unique<
      diagnostics::AsyncGrpcClient<diagnostics::grpc_api::Diagnosticsd>>(
      message_loop_.task_runner(), kDiagnosticsdGrpcUri);
  VLOG(0) << "Created gRPC diagnosticsd client on " << kDiagnosticsdGrpcUri;

  // Send a test RPC and print out the response.
  RpcReply<diagnostics::grpc_api::GetProcDataResponse> rpc_reply;
  diagnostics::grpc_api::GetProcDataRequest request;
  request.set_type(diagnostics::grpc_api::GetProcDataRequest::FILE_MEMINFO);
  diagnosticsd_grpc_client_->CallRpc(
      &diagnostics::grpc_api::Diagnosticsd::Stub::AsyncGetProcData, request,
      rpc_reply.MakeWriter());
  VLOG(0) << "Sent GetProcDataRequest";

  rpc_reply.Wait();
  if (rpc_reply.IsError()) {
    VLOG(0) << "RPC Response Error!";
  } else {
    VLOG(0) << "RPC Response Good: "
            << rpc_reply.response().file_dump()[0].path() << " "
            << rpc_reply.response().file_dump()[0].contents();
  }

  // Shut down the AsyncGrpcClient.
  base::RunLoop loop;
  diagnosticsd_grpc_client_->Shutdown(loop.QuitClosure());
  loop.Run();

  return 0;
}
