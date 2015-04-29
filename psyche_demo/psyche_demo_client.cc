// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sysexits.h>

#include <memory>
#include <utility>

#include <base/bind.h>
#include <base/logging.h>
#include <base/macros.h>
#include <base/memory/weak_ptr.h>
#include <base/message_loop/message_loop.h>
#include <base/timer/timer.h>
#include <chromeos/flag_helper.h>
#include <protobinder/binder_proxy.h>
#include <psyche/psyche_connection.h>
#include <psyche/psyche_daemon.h>

#include "psyche_demo/constants.h"
#include "psyche_demo/proto_bindings/psyche_demo.pb.h"
#include "psyche_demo/proto_bindings/psyche_demo.pb.rpc.h"

using protobinder::BinderProxy;

namespace psyche {
namespace {

class DemoClient : public PsycheDaemon {
 public:
  explicit DemoClient(const std::string& service_name)
      : service_name_(service_name),
        token_(0),
        weak_ptr_factory_(this) {}
  ~DemoClient() override = default;

 private:
  // Delay between calls to SendPing().
  const int kPingIntervalMs = 1000;

  void ReceiveService(std::unique_ptr<BinderProxy> proxy) {
    LOG(INFO) << "Received service with handle " << proxy->handle();
    proxy_ = std::move(proxy);
    server_.reset(protobinder::BinderToInterface<psyche::IPsycheDemoServer>(
        proxy_.get()));
    timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(kPingIntervalMs),
                 this, &DemoClient::SendPing);
    SendPing();
  }

  // Calls |server_|'s Ping method.
  void SendPing() {
    DCHECK(server_);
    token_++;
    LOG(INFO) << "Sending " << token_ << " to server";
    psyche::PingRequest request;
    request.set_token(token_);
    psyche::PingResponse response;
    const int result = server_->Ping(&request, &response);
    if (result != 0)
      LOG(ERROR) << "Request yielded result of " << result;
    else
      LOG(INFO) << "Got " << response.token() << " from server";
  }

  // PsycheDaemon:
  int OnInit() override {
    int return_code = PsycheDaemon::OnInit();
    if (return_code != EX_OK)
      return return_code;

    LOG(INFO) << "Requesting service " << service_name_;
    CHECK(psyche_connection()->GetService(
        service_name_, base::Bind(&DemoClient::ReceiveService,
                                  weak_ptr_factory_.GetWeakPtr())));
    return EX_OK;
  }

  std::unique_ptr<BinderProxy> proxy_;
  std::unique_ptr<psyche::IPsycheDemoServer> server_;

  // Name of service to send requests to.
  std::string service_name_;

  // Token included in ping requests.
  int token_;

  // Runs SendPing() periodically.
  base::RepeatingTimer<DemoClient> timer_;

  // Keep this member last.
  base::WeakPtrFactory<DemoClient> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DemoClient);
};

}  // namespace
}  // namespace psyche

int main(int argc, char* argv[]) {
  DEFINE_string(service_name, psyche::kDefaultService,
                "Name of service to communicate with");
  chromeos::FlagHelper::Init(
      argc, argv, "Example client that communicates with psyched.");

  return psyche::DemoClient(FLAGS_service_name).Run();
}
