// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <brillo/syslog_logging.h>

#include <memory>

#include <base/command_line.h>
#include <base/files/file_path.h>
#include <base/logging.h>
#include <base/macros.h>

#include "arc/vm/vsock_proxy/proxy_file_system.h"
#include "arc/vm/vsock_proxy/proxy_service.h"
#include "arc/vm/vsock_proxy/server_proxy.h"

int main(int argc, char** argv) {
  // Initialize CommandLine for VLOG before InitLog.
  base::CommandLine::Init(argc, argv);
  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogHeader |
                  brillo::kLogToStderrIfTty);

  if (argc < 2) {
    LOG(ERROR) << "Mount path is not specified.";
    return 1;
  }
  // ProxyService for ServerProxy will be started after FUSE initialization is
  // done. See also ProxyFileSystem::Init().
  arc::ProxyFileSystem file_system{base::FilePath(argv[1])};
  return file_system.Run(std::make_unique<arc::ProxyService>(
      std::make_unique<arc::ServerProxy>(&file_system)));
}
