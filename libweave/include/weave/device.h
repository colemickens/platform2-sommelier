// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_INCLUDE_WEAVE_DEVICE_H_
#define LIBWEAVE_INCLUDE_WEAVE_DEVICE_H_

#include <memory>
#include <set>
#include <string>

#include <weave/cloud.h>
#include <weave/commands.h>
#include <weave/config_store.h>
#include <weave/export.h>
#include <weave/http_client.h>
#include <weave/http_server.h>
#include <weave/mdns.h>
#include <weave/network.h>
#include <weave/privet.h>
#include <weave/state.h>
#include <weave/task_runner.h>

namespace weave {

class Device {
 public:
  struct Options {
    bool xmpp_enabled = true;
    bool disable_privet = false;
    bool disable_security = false;
    bool enable_ping = false;
    std::string test_privet_ssid;
  };

  virtual ~Device() = default;

  virtual void Start(const Options& options,
                     ConfigStore* config_store,
                     TaskRunner* task_runner,
                     HttpClient* http_client,
                     Network* network,
                     Mdns* mdns,
                     HttpServer* http_server) = 0;

  virtual Commands* GetCommands() = 0;
  virtual State* GetState() = 0;
  virtual Cloud* GetCloud() = 0;
  virtual Privet* GetPrivet() = 0;

  LIBWEAVE_EXPORT static std::unique_ptr<Device> Create();
};

}  // namespace weave

#endif  // LIBWEAVE_INCLUDE_WEAVE_DEVICE_H_
