// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sysexits.h>

#include <base/bind.h>
#include <base/logging.h>
#include <base/macros.h>
#include <base/message_loop/message_loop.h>
#include <base/timer/timer.h>
#include <chromeos/daemons/daemon.h>
#include <chromeos/flag_helper.h>
#include <protobinder/binder_manager.h>
#include <protobinder/binder_proxy.h>
#include <protobinder/iinterface.h>
#include <psyche/psyche_connection.h>

#include "psyche_demo/proto_bindings/psyche_demo.pb.h"
#include "psyche_demo/proto_bindings/psyche_demo.pb.rpc.h"

using protobinder::BinderManagerInterface;
using protobinder::BinderProxy;

namespace {

class Daemon : public chromeos::Daemon,
               public base::MessageLoopForIO::Watcher {
 public:
  explicit Daemon(const std::string& service_name)
      : service_name_(service_name),
        token_(0) {}
  ~Daemon() override = default;

 private:
  // Delay between calls to SendPing().
  const int kPingIntervalMs = 1000;

  void RequestService() {
    LOG(INFO) << "Requesting service " << service_name_;
    psyche_.GetService(service_name_,
        base::Bind(&Daemon::ReceiveService, base::Unretained(this)));
  }

  void ReceiveService(scoped_ptr<BinderProxy> proxy) {
    LOG(INFO) << "Received service with handle " << proxy->handle();
    proxy_ = proxy.Pass();
    server_.reset(protobinder::BinderToInterface<psyche::IPsycheDemoServer>(
        proxy_.get()));
    timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(kPingIntervalMs),
                 this, &Daemon::SendPing);
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

  // chromeos::Daemon:
  int OnInit() override {
    const int return_code = chromeos::Daemon::OnInit();
    if (return_code != EX_OK)
      return return_code;

    int binder_fd = 0;
    CHECK(BinderManagerInterface::Get()->GetFdForPolling(&binder_fd));
    CHECK(base::MessageLoopForIO::current()->WatchFileDescriptor(
        binder_fd, true /* persistent */, base::MessageLoopForIO::WATCH_READ,
        &fd_watcher_, this));

    CHECK(psyche_.Init());
    RequestService();
    return EX_OK;
  }

  // MessageLoopForIO::Watcher:
  void OnFileCanReadWithoutBlocking(int file_descriptor) override {
    BinderManagerInterface::Get()->HandleEvent();
  }
  void OnFileCanWriteWithoutBlocking(int file_descriptor) override {
    NOTREACHED();
  }

  base::MessageLoopForIO::FileDescriptorWatcher fd_watcher_;
  psyche::PsycheConnection psyche_;
  scoped_ptr<BinderProxy> proxy_;
  scoped_ptr<psyche::IPsycheDemoServer> server_;

  // Name of service to send requests to.
  std::string service_name_;

  // Token included in ping requests.
  int token_;

  // Runs SendPing() periodically.
  base::RepeatingTimer<Daemon> timer_;

  DISALLOW_COPY_AND_ASSIGN(Daemon);
};

}  // namespace

int main(int argc, char* argv[]) {
  DEFINE_string(service_name, "psyche_demo_server",
                "Name of service to communicate with");
  chromeos::FlagHelper::Init(
      argc, argv, "Example client that communicates with psyched.");
  return Daemon(FLAGS_service_name).Run();
}
