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

#include "diagnostics/constants/grpc_constants.h"
#include "diagnostics/dpsl/internal/callback_utils.h"
#include "diagnostics/dpsl/public/dpsl_rpc_handler.h"
#include "diagnostics/dpsl/public/dpsl_thread_context.h"

namespace diagnostics {

namespace {

std::string GetWilcoDtcGrpcUri(DpslRpcServer::GrpcServerUri grpc_server_uri) {
  switch (grpc_server_uri) {
    case DpslRpcServer::GrpcServerUri::kVmVsock:
      return GetWilcoDtcGrpcGuestVsockUri();
    case DpslRpcServer::GrpcServerUri::kUiMessageReceiverVmVsock:
      return GetUiMessageReceiverWilcoDtcGrpcGuestVsockUri();
  }
  NOTREACHED() << "Unexpected GrpcServerUri: "
               << static_cast<int>(grpc_server_uri);
  return "";
}

}  // namespace

DpslRpcServerImpl::DpslRpcServerImpl(DpslRpcHandler* rpc_handler,
                                     GrpcServerUri grpc_server_uri,
                                     const std::string& grpc_server_uri_string)
    : rpc_handler_(rpc_handler),
      async_grpc_server_(base::ThreadTaskRunnerHandle::Get(),
                         {grpc_server_uri_string}) {
  DCHECK(rpc_handler_);
  auto handle_message_from_ui_handler =
      &DpslRpcServerImpl::HandleMessageFromUiStub;
  switch (grpc_server_uri) {
    case GrpcServerUri::kVmVsock:
      break;
    case GrpcServerUri::kUiMessageReceiverVmVsock:
      handle_message_from_ui_handler = &DpslRpcServerImpl::HandleMessageFromUi;
      break;
  }
  async_grpc_server_.RegisterHandler(
      &grpc_api::WilcoDtc::AsyncService::RequestHandleMessageFromUi,
      base::Bind(handle_message_from_ui_handler, base::Unretained(this)));

  async_grpc_server_.RegisterHandler(
      &grpc_api::WilcoDtc::AsyncService::RequestHandleEcNotification,
      base::Bind(&DpslRpcServerImpl::HandleEcNotification,
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

void DpslRpcServerImpl::HandleMessageFromUiStub(
    std::unique_ptr<grpc_api::HandleMessageFromUiRequest> request,
    const HandleMessageFromUiCallback& callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  callback.Run(nullptr /* response */);
}

void DpslRpcServerImpl::HandleEcNotification(
    std::unique_ptr<grpc_api::HandleEcNotificationRequest> request,
    const HandleEcNotificationCallback& callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  rpc_handler_->HandleEcNotification(
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
      rpc_handler, grpc_server_uri, GetWilcoDtcGrpcUri(grpc_server_uri));
  if (!dpsl_rpc_server_impl->Init())
    return nullptr;
  return dpsl_rpc_server_impl;
}

}  // namespace diagnostics
