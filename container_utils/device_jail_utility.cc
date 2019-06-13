// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <linux/device_jail.h>
#include <stdio.h>

#include <memory>
#include <string>

#include <base/compiler_specific.h>
#include <base/logging.h>
#include <base/message_loop/message_loop.h>
#include <base/run_loop.h>
#include <brillo/flag_helper.h>

#include "container_utils/device_jail_control.h"
#include "container_utils/device_jail_server.h"

using device_jail::DeviceJailControl;
using device_jail::DeviceJailServer;

namespace {

class RequestHandler : public DeviceJailServer::Delegate {
 public:
  jail_request_result HandleRequest(const std::string& path) override {
    printf("Request for device %s ([a]llow, [D]eny, [l]ockdown, d[e]tach)\n",
           path.c_str());
    char buf[64];
    int ret = read(0, buf, sizeof(buf));
    if (ret <= 0 || ret > 2)
      return JAIL_REQUEST_DENY;

    switch (buf[0]) {
    case 'a':
      return JAIL_REQUEST_ALLOW;
    case 'd':
      return JAIL_REQUEST_DENY;
    case 'l':
      return JAIL_REQUEST_ALLOW_WITH_LOCKDOWN;
    case 'e':
      return JAIL_REQUEST_ALLOW_WITH_DETACH;
    default:
      printf("Unrecognized command\n");
      return JAIL_REQUEST_DENY;
    }
  }
};

}  // namespace

int main(int argc, char** argv) {
  DEFINE_string(add, "", "Path to device to jail.");
  DEFINE_string(remove, "", "Path to jail device to remove.");
  DEFINE_bool(server, false, "Enable server mode.");
  brillo::FlagHelper::Init(argc, argv, "device_jail utility program");

  bool add_mode = !FLAGS_add.empty();
  bool remove_mode = !FLAGS_remove.empty();

  if (add_mode + remove_mode + FLAGS_server != 1)
    LOG(FATAL) << "can only have one flag";

  if (FLAGS_server) {
    base::MessageLoopForIO message_loop;
    std::unique_ptr<DeviceJailServer> server =
        DeviceJailServer::CreateAndListen(std::make_unique<RequestHandler>(),
                                          &message_loop);
    if (!server)
      LOG(FATAL) << "could not initialize device jail server";
    base::RunLoop().Run();
    return 0;
  }

  std::unique_ptr<DeviceJailControl> jail_control =
      DeviceJailControl::Create();
  if (!jail_control)
    LOG(FATAL) << "could not initialize device jail control";

  if (add_mode) {
    std::string jail_path;
    switch (jail_control->AddDevice(FLAGS_add, &jail_path)) {
    case DeviceJailControl::AddResult::ERROR:
      LOG(FATAL) << "could not create jail device";
      NOTREACHED();
      // TODO(denik): Remove break after fall-through check
      // is fixed with NOTREACHED().
      break;
    case DeviceJailControl::AddResult::ALREADY_EXISTS:
      LOG(INFO) << "jail already exists at " << jail_path;
      break;
    case DeviceJailControl::AddResult::CREATED:
      LOG(INFO) << "created jail at " << jail_path;
      break;
    }
  } else if (remove_mode && !jail_control->RemoveDevice(FLAGS_remove)) {
    LOG(FATAL) << "could not remove device";
  }

  return 0;
}
