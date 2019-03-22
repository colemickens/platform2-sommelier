// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <brillo/syslog_logging.h>

#include <memory>

#include <base/files/file_path.h>
#include <base/logging.h>
#include <base/macros.h>

#include "arc/vm/vsock_proxy/proxy_base.h"
#include "arc/vm/vsock_proxy/proxy_file_system.h"
#include "arc/vm/vsock_proxy/proxy_service.h"
#include "arc/vm/vsock_proxy/server_proxy.h"
#include "arc/vm/vsock_proxy/server_proxy_file_system.h"

namespace arc {
namespace {

// Adaper to inject ServerProxy creation to ProxyService.
class ServerProxyFactory : public ProxyService::ProxyFactory {
 public:
  explicit ServerProxyFactory(ProxyFileSystem* file_system)
      : file_system_(file_system) {}
  ~ServerProxyFactory() override = default;

  std::unique_ptr<ProxyBase> Create() override {
    return std::make_unique<ServerProxy>(file_system_);
  }

 private:
  ProxyFileSystem* const file_system_;

  DISALLOW_COPY_AND_ASSIGN(ServerProxyFactory);
};

}  // namespace
}  // namespace arc

int main(int argc, char** argv) {
  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogHeader |
                  brillo::kLogToStderrIfTty);

  if (argc < 2) {
    LOG(ERROR) << "Mount path is not specified.";
    return 1;
  }

  // ProxyService for ServerProxy will be started after FUSE initialization is
  // done. See also ServerProxyFileSystem::Init().
  arc::ServerProxyFileSystem file_system{base::FilePath(argv[1])};
  return file_system.Run(
      std::make_unique<arc::ServerProxyFactory>(&file_system));
}
