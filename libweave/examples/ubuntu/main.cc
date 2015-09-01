// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <weave/device.h>

#include "libweave/examples/ubuntu/avahi_client.h"
#include "libweave/examples/ubuntu/curl_http_client.h"
#include "libweave/examples/ubuntu/event_http_server.h"
#include "libweave/examples/ubuntu/event_task_runner.h"
#include "libweave/examples/ubuntu/file_config_store.h"
#include "libweave/examples/ubuntu/network_manager.h"

int main() {
  weave::examples::FileConfigStore config_store;
  weave::examples::EventTaskRunner task_runner;
  weave::examples::CurlHttpClient http_client{&task_runner};
  weave::examples::NetworkImpl network{&task_runner};
  weave::examples::MdnsImpl mdns;
  weave::examples::HttpServerImpl http_server{task_runner.GetEventBase()};

  auto device = weave::Device::Create();
  weave::Device::Options opts;
  opts.xmpp_enabled = true;
  opts.disable_privet = false;
  opts.disable_security = false;
  opts.enable_ping = true;
  device->Start(opts, &config_store, &task_runner, &http_client, &network,
                &mdns, &http_server);

  task_runner.Run();

  LOG(INFO) << "exit";
  return 0;
}
