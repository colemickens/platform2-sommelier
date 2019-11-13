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

namespace {

class ProxyFileSystemDelegate : public arc::ProxyFileSystem::Delegate {
 public:
  explicit ProxyFileSystemDelegate(const base::FilePath& mount_path)
      : file_system_(this, mount_path) {}
  ~ProxyFileSystemDelegate() override = default;
  ProxyFileSystemDelegate(const ProxyFileSystemDelegate&) = delete;
  ProxyFileSystemDelegate& operator=(const ProxyFileSystemDelegate&) = delete;

  int Run() {
    auto server_proxy = std::make_unique<arc::ServerProxy>(&file_system_);
    server_proxy_ = server_proxy.get();
    return file_system_.Run(
        std::make_unique<arc::ProxyService>(std::move(server_proxy)));
  }

  // ProxyFileSystem::Delegate overrides:
  void Pread(int64_t handle,
             uint64_t count,
             uint64_t offset,
             PreadCallback callback) override {
    server_proxy_->vsock_proxy()->Pread(handle, count, offset,
                                        std::move(callback));
  }
  void Close(int64_t handle) override {
    server_proxy_->vsock_proxy()->Close(handle);
  }
  void Fstat(int64_t handle, FstatCallback callback) override {
    server_proxy_->vsock_proxy()->Fstat(handle, std::move(callback));
  }

 private:
  arc::ProxyFileSystem file_system_;
  arc::ServerProxy* server_proxy_ = nullptr;
};

}  // namespace

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
  ProxyFileSystemDelegate file_system_delegate{base::FilePath(argv[1])};
  return file_system_delegate.Run();
}
