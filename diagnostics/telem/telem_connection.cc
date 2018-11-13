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
template <typename ResponseType>
void OnRpcResponseReceived(std::unique_ptr<ResponseType>* response_destination,
                           base::Closure run_loop_quit_closure,
                           std::unique_ptr<ResponseType> response) {
  *response_destination = std::move(response);
  run_loop_quit_closure.Run();
}

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
  grpc_api::GetProcDataRequest request;
  request.set_type(type);
  base::RunLoop run_loop;
  std::unique_ptr<diagnostics::grpc_api::GetProcDataResponse> response;
  client_->CallRpc(
      &diagnostics::grpc_api::Diagnosticsd::Stub::AsyncGetProcData, request,
      base::Bind(
          &OnRpcResponseReceived<diagnostics::grpc_api::GetProcDataResponse>,
          base::Unretained(&response), run_loop.QuitClosure()));
  VLOG(0) << "Sent GetProcDataRequest";
  run_loop.Run();

  // When reading a single file, we expect a single file_dump response.
  if (!response || response->file_dump_size() != 1) {
    VLOG(0) << "RPC Response Error!";
  } else {
    VLOG(0) << "RPC Response Good: " << response->file_dump(0).path() << " "
            << response->file_dump(0).contents();
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
