// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/dpsl/internal/dpsl_rpc_server_impl.h"

#include <functional>
#include <utility>

#include <base/bind.h>
#include <base/location.h>
#include <base/logging.h>
#include <base/memory/ref_counted.h>
#include <base/run_loop.h>
#include <base/threading/thread_task_runner_handle.h>

#include "diagnostics/dpsl/internal/callback_utils.h"
#include "diagnostics/dpsl/public/dpsl_rpc_handler.h"
#include "diagnostics/dpsl/public/dpsl_thread_context.h"

namespace diagnostics {

namespace {

// Predefined paths on which the DiagnosticsProcessor gRPC server started by
// DPSL may be listening:
constexpr char kDiagnosticsProcessorLocalDomainSocketGrpcUri[] =
    "unix:/run/diagnostics/grpc_sockets/diagnostics_processor_socket";

std::string GetDiagnosticsProcessorGrpcUri(
    DpslRpcServer::GrpcServerUri grpc_server_uri) {
  switch (grpc_server_uri) {
    case DpslRpcServer::GrpcServerUri::kLocalDomainSocket:
      return kDiagnosticsProcessorLocalDomainSocketGrpcUri;
  }
  NOTREACHED() << "Unexpected GrpcServerUri: "
               << static_cast<int>(grpc_server_uri);
  return "";
}

}  // namespace

DpslRpcServerImpl::DpslRpcServerImpl(DpslRpcHandler* rpc_handler,
                                     const std::string& server_grpc_uri)
    : rpc_handler_(rpc_handler),
      async_grpc_server_(base::ThreadTaskRunnerHandle::Get(), server_grpc_uri) {
  DCHECK(rpc_handler_);
  async_grpc_server_.RegisterHandler(
      &grpc_api::DiagnosticsProcessor::AsyncService::RequestHandleMessageFromUi,
      base::Bind(&DpslRpcServerImpl::HandleMessageFromUi,
                 base::Unretained(this)));
}

DpslRpcServerImpl::~DpslRpcServerImpl() {
  CHECK(sequence_checker_.CalledOnValidSequence());

  base::RunLoop run_loop;
  async_grpc_server_.Shutdown(run_loop.QuitClosure());
  run_loop.Run();
}

bool DpslRpcServerImpl::Init() {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  return async_grpc_server_.Start();
}

void DpslRpcServerImpl::HandleMessageFromUi(
    std::unique_ptr<grpc_api::HandleMessageFromUiRequest> request,
    const HandleMessageFromUiCallback& callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  rpc_handler_->HandleMessageFromUi(
      std::move(request),
      MakeStdFunctionFromCallback(
          MakeOriginTaskRunnerPostingCallback(FROM_HERE, callback)));
}

// static
std::unique_ptr<DpslRpcServer> DpslRpcServer::Create(
    DpslThreadContext* thread_context,
    DpslRpcHandler* rpc_handler,
    GrpcServerUri grpc_server_uri) {
  CHECK(thread_context);
  CHECK(rpc_handler);
  CHECK(thread_context->BelongsToCurrentThread());

  auto dpsl_rpc_server_impl = std::make_unique<DpslRpcServerImpl>(
      rpc_handler, GetDiagnosticsProcessorGrpcUri(grpc_server_uri));
  if (!dpsl_rpc_server_impl->Init())
    return nullptr;
  return dpsl_rpc_server_impl;
}

}  // namespace diagnostics
