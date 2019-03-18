// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <unistd.h>

#include <memory>

#include <base/logging.h>
#include <base/macros.h>
#include <brillo/syslog_logging.h>

#include "arc/vm/vsock_proxy/proxy_service.h"
#include "arc/vm/vsock_proxy/server_proxy.h"

namespace {

// Adaper to inject ServerProxy creation to ProxyService.
class ServerProxyFactory : public arc::ProxyService::ProxyFactory {
 public:
  ServerProxyFactory() = default;
  ~ServerProxyFactory() override = default;

  std::unique_ptr<arc::ProxyBase> Create() override {
    return std::make_unique<arc::ServerProxy>();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ServerProxyFactory);
};

}  // namespace

int main() {
  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogHeader |
                  brillo::kLogToStderrIfTty);

  arc::ProxyService service(std::make_unique<ServerProxyFactory>());
  if (!service.Start()) {
    LOG(ERROR) << "Failed to start ServerProxy.";
    return 1;
  }
  LOG(INFO) << "ServerProxy has been started successfully.";

  // Sleep forever.
  // TODO(hidehiko): On main thread, Fuse handler will run.
  while (1)
    pause();
  return 0;
}
