// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include <base/bind.h>
#include <base/callback.h>
#include <base/logging.h>
#include <base/memory/weak_ptr.h>
#include <base/run_loop.h>
#include <base/threading/thread_task_runner_handle.h>

#include "diagnostics/telem/telem_connection.h"
#include "diagnosticsd.pb.h"  // NOLINT(build/include)

namespace {
// A utility for testing outgoing RPCs. It gets notified of a response to an
// outgoing RPC through the callback it returns from |MakeWriter|.
template <typename ResponseType>
class RpcReply {
 public:
  RpcReply() : weak_ptr_factory_(this) {}
  ~RpcReply() = default;

  // Returns a callback that should be called when a response to the outgoing
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
}  // namespace

namespace diagnostics {

TelemConnection::TelemConnection(const std::string& target_uri)
    : target_uri_(target_uri) {}

TelemConnection::~TelemConnection() {
  ShutdownClient();
}

void TelemConnection::Connect() {
  if (!client_) {
    client_ = std::make_unique<AsyncGrpcClient<grpc_api::Diagnosticsd>>(
        base::ThreadTaskRunnerHandle::Get(), target_uri_);
    VLOG(0) << "Created gRPC diagnosticsd client on " << target_uri_;
  } else {
    VLOG(0) << "gRPC diagnosticsd client already exists.";
  }
}

void TelemConnection::GetItem(TelemetryItemEnum item) {
  switch (item) {
    case TelemetryItemEnum::kMemInfo:
      GetProcMessage(grpc_api::GetProcDataRequest::FILE_MEMINFO);
      break;
    case TelemetryItemEnum::KAcpiButton:
      GetProcMessage(grpc_api::GetProcDataRequest::DIRECTORY_ACPI_BUTTON);
      break;
    default:
      LOG(ERROR) << "Undefined telemetry item: " << item;
  }
}

void TelemConnection::GetProcMessage(grpc_api::GetProcDataRequest::Type type) {
  switch (type) {
    case grpc_api::GetProcDataRequest::FILE_MEMINFO:
      GetProcFile(type);
      break;
    case grpc_api::GetProcDataRequest::DIRECTORY_ACPI_BUTTON:
      GetProcDirectory(type);
      break;
    // TODO(pmoy@chromium.org): Add the rest of the GetProcDataRequest types.
    default:
      LOG(ERROR) << "GetProcData gRPC request type unset or invalid: " << type;
  }
}

void TelemConnection::GetProcFile(grpc_api::GetProcDataRequest::Type type) {
  // Send a test RPC and print out the response.
  RpcReply<grpc_api::GetProcDataResponse> rpc_reply;
  grpc_api::GetProcDataRequest request;
  request.set_type(type);
  client_->CallRpc(&grpc_api::Diagnosticsd::Stub::AsyncGetProcData, request,
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
}

void TelemConnection::GetProcDirectory(
    grpc_api::GetProcDataRequest::Type type) {
  // TODO(pmoy@chromium.org): Add a method for reading directories.
  NOTIMPLEMENTED();
}

void TelemConnection::ShutdownClient() {
  base::RunLoop loop;
  client_->Shutdown(loop.QuitClosure());
  loop.Run();
}

}  // namespace diagnostics
