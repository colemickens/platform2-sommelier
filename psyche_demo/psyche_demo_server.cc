// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/logging.h>
#include <base/macros.h>
#include <chromeos/flag_helper.h>
#include <protobinder/binder_daemon.h>
#include <psyche/psyche_connection.h>

#include "psyche_demo/proto_bindings/psyche_demo.pb.h"
#include "psyche_demo/proto_bindings/psyche_demo.pb.rpc.h"

using psyche::PsycheConnection;

namespace psyche {
namespace {

class DemoServer : public IPsycheDemoServerHostInterface {
 public:
  DemoServer() = default;
  ~DemoServer() override = default;

  // IPsychedDemoServerInterface:
  int Ping(PingRequest* in, PingResponse* out) override {
    LOG(INFO) << "Received ping request with token " << in->token() << "";
    out->set_token(in->token());
    return 0;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DemoServer);
};

}  // namespace
}  // namespace psyche

int main(int argc, char* argv[]) {
  DEFINE_string(service_name, "psyche_demo_server",
                "Service name to register with psyche");
  chromeos::FlagHelper::Init(
      argc, argv, "Example server that registers with psyched.");

  PsycheConnection connection;
  CHECK(connection.Init());
  scoped_ptr<psyche::DemoServer> server(new psyche::DemoServer);
  CHECK(connection.RegisterService(FLAGS_service_name, server.get()));
  return protobinder::BinderDaemon("foo", server.Pass()).Run();
}
