// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sysexits.h>

#include <base/logging.h>
#include <base/macros.h>
#include <chromeos/flag_helper.h>
#include <psyche/psyche_connection.h>
#include <psyche/psyche_daemon.h>

#include "psyche_demo/constants.h"
#include "psyche_demo/proto_bindings/psyche_demo.pb.h"
#include "psyche_demo/proto_bindings/psyche_demo.pb.rpc.h"

namespace psyche {
namespace {

class DemoServer : public PsycheDaemon,
                   public IPsycheDemoServerHostInterface {
 public:
  explicit DemoServer(const std::string& service_name)
      : service_name_(service_name) {}
  ~DemoServer() override = default;

  // PsycheDaemon:
  int OnInit() override {
    int return_code = PsycheDaemon::OnInit();
    if (return_code != EX_OK)
      return return_code;

    if (!psyche_connection()->RegisterService(service_name_, this))
      return -1;
    return 0;
  }

  // IPsychedDemoServerInterface:
  int Ping(PingRequest* in, PingResponse* out) override {
    LOG(INFO) << "Received ping request with token " << in->token() << "";
    out->set_token(in->token());
    return 0;
  }

 private:
  // Name of service to register with psyched.
  std::string service_name_;

  DISALLOW_COPY_AND_ASSIGN(DemoServer);
};

}  // namespace
}  // namespace psyche

int main(int argc, char* argv[]) {
  DEFINE_string(service_name, psyche::kDefaultService,
                "Service name to register with psyche");
  chromeos::FlagHelper::Init(
      argc, argv, "Example server that registers with psyched.");

  return psyche::DemoServer(FLAGS_service_name).Run();
}
