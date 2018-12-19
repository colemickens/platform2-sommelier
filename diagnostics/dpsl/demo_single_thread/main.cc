// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Demo for a diagnostics_processor program built using the DPSL library.
//
// The demo functionality: waits for a message from UI, and, when receiving one,
// fetches system uptime (/proc/uptime) and sends it back to the UI.
//
// The core logic in the demo is single-threaded and asynchronous.

#include <algorithm>
#include <functional>
#include <iostream>
#include <memory>

#include "diagnostics/dpsl/public/dpsl_global_context.h"
#include "diagnostics/dpsl/public/dpsl_requester.h"
#include "diagnostics/dpsl/public/dpsl_rpc_handler.h"
#include "diagnostics/dpsl/public/dpsl_rpc_server.h"
#include "diagnostics/dpsl/public/dpsl_thread_context.h"

#include "diagnostics_processor.pb.h"  // NOLINT(build/include)

namespace {

class DemoRpcHandler final : public diagnostics::DpslRpcHandler {
 public:
  explicit DemoRpcHandler(diagnostics::DpslThreadContext* thread_context);
  ~DemoRpcHandler() override;

  // DpslServer overrides:
  void HandleMessageFromUi(
      std::unique_ptr<diagnostics::grpc_api::HandleMessageFromUiRequest>
          request,
      HandleMessageFromUiCallback callback) override;
  void HandleEcNotification(
      std::unique_ptr<diagnostics::grpc_api::HandleEcNotificationRequest>
          request,
      HandleEcNotificationCallback callback) override;

 private:
  void FetchSystemUptime();
  void OnSystemUptimeFetched(
      std::unique_ptr<diagnostics::grpc_api::GetProcDataResponse> response);
  void SendUptimeMessageToUi(const std::string& system_uptime);

  std::unique_ptr<diagnostics::DpslRequester> requester_;
};

DemoRpcHandler::DemoRpcHandler(diagnostics::DpslThreadContext* thread_context)
    : requester_(diagnostics::DpslRequester::Create(
          thread_context,
          diagnostics::DpslRequester::GrpcClientUri::kLocalDomainSocket)) {}

DemoRpcHandler::~DemoRpcHandler() = default;

void DemoRpcHandler::HandleMessageFromUi(
    std::unique_ptr<diagnostics::grpc_api::HandleMessageFromUiRequest> request,
    HandleMessageFromUiCallback callback) {
  std::cerr << "Received HandleMessageFromUi request: "
            << request->json_message() << std::endl;
  // Note: every incoming RPC must be answered.
  callback(
      std::make_unique<diagnostics::grpc_api::HandleMessageFromUiResponse>());
  FetchSystemUptime();
}

void DemoRpcHandler::HandleEcNotification(
    std::unique_ptr<diagnostics::grpc_api::HandleEcNotificationRequest> request,
    HandleEcNotificationCallback callback) {
  std::cerr << "Received HandleEcNotification request: " << request->payload()
            << std::endl;
  // Note: every incoming RPC must be answered.
  callback(
      std::make_unique<diagnostics::grpc_api::HandleEcNotificationResponse>());
  FetchSystemUptime();
}

void DemoRpcHandler::FetchSystemUptime() {
  auto request = std::make_unique<diagnostics::grpc_api::GetProcDataRequest>();
  request->set_type(diagnostics::grpc_api::GetProcDataRequest::FILE_UPTIME);
  requester_->GetProcData(std::move(request),
                          std::bind(&DemoRpcHandler::OnSystemUptimeFetched,
                                    this, std::placeholders::_1));
}

void DemoRpcHandler::OnSystemUptimeFetched(
    std::unique_ptr<diagnostics::grpc_api::GetProcDataResponse> response) {
  std::string system_uptime;
  if (response && response->file_dump_size()) {
    system_uptime = response->file_dump(0).contents();
    std::replace(system_uptime.begin(), system_uptime.end(), '\n', ' ');
  }
  std::cerr << "Fetched system uptime: " << system_uptime << std::endl;
  SendUptimeMessageToUi(system_uptime);
}

void DemoRpcHandler::SendUptimeMessageToUi(const std::string& system_uptime) {
  auto request =
      std::make_unique<diagnostics::grpc_api::SendMessageToUiRequest>();
  request->set_json_message("{\"uptime\": \"" + system_uptime + "\"}");
  requester_->SendMessageToUi(
      std::move(request),
      [](std::unique_ptr<diagnostics::grpc_api::SendMessageToUiResponse>
             response) {
        std::cerr << "Sent 'uptime' SendMessageToUi" << std::endl;
      });
}

}  // namespace

int main() {
  // Note: this object must outlive all objects it was passed to (i.e., all
  // other DPSL objects).
  auto global_context = diagnostics::DpslGlobalContext::Create();

  // Note: this object must outlive all objects it was passed to (i.e., all
  // other DPSL objects belonging to the same thread).
  auto thread_context =
      diagnostics::DpslThreadContext::Create(global_context.get());

  DemoRpcHandler demo_rpc_handler(thread_context.get());
  auto rpc_server = diagnostics::DpslRpcServer::Create(
      thread_context.get(), &demo_rpc_handler,
      diagnostics::DpslRpcServer::GrpcServerUri::kLocalDomainSocket);
  // This blocks (forever in this program, since it never calls
  // QuitEventLoop()).
  thread_context->RunEventLoop();
}
