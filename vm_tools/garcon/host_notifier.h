// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_GARCON_HOST_NOTIFIER_H_
#define VM_TOOLS_GARCON_HOST_NOTIFIER_H_

#include <memory>
#include <string>
#include <vector>

#include <base/files/file_path_watcher.h>
#include <base/files/scoped_file.h>
#include <base/macros.h>
#include <base/memory/weak_ptr.h>
#include <base/message_loop/message_loop.h>
#include <grpc++/grpc++.h>

#include "container_host.grpc.pb.h"  // NOLINT(build/include)
#include "vm_tools/garcon/package_kit_proxy.h"

namespace vm_tools {
namespace garcon {

// Handles making calls to cicerone running in the host.
class HostNotifier : public base::MessageLoopForIO::Watcher,
                     public PackageKitProxy::PackageKitObserver {
 public:
  // Creates and inits the HostNotifier for running on the current sequence.
  // Returns null if there was any failure.
  static std::unique_ptr<HostNotifier> Create(base::Closure shutdown_closure);

  // Sends a gRPC call to the host to notify it to open the specified URL with
  // the web browser. Returns true on success, false otherwise.
  static bool OpenUrlInHost(const std::string& url);

  ~HostNotifier();

  // Notifies the host that garcon is ready. This will send the initial update
  // for the application list and also establish a watcher for any updates to
  // the list of installed applications. Returns false if there was any failure.
  bool Init();

  // base::MessageLoopForIO::Watcher overrides.
  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override;

  // vm_tools::garcon::PackageKitObserver overrides.
  void OnInstallCompletion(bool success,
                           const std::string& failure_reason) override;
  void OnInstallProgress(
      vm_tools::container::InstallLinuxPackageProgressInfo::Status status,
      uint32_t percent_progress) override;

  // Returns a WeakPtr reference to this object.
  base::WeakPtr<HostNotifier> GetWeakPtr();

  // Sets the gRPC Server object which will then be shutdown when this thread
  // detects a SIGTERM.
  void set_grpc_server(std::shared_ptr<grpc::Server> grpc_server) {
    grpc_server_ = grpc_server;
  }

 private:
  explicit HostNotifier(base::Closure shutdown_closure);

  // Sends a message to the host indicating that our server is ready for
  // accepting incoming calls.
  bool NotifyHostGarconIsReady();

  // Sends a message to the host indicating the container is shutting down.
  void NotifyHostOfContainerShutdown();

  // Sends a list of the installed applications to the host.
  void SendAppListToHost();

  // Callback for when desktop file path changes occur.
  void DesktopPathsChanged(const base::FilePath& path, bool error);

  // gRPC stub for communicating with cicerone on the host.
  std::unique_ptr<vm_tools::container::ContainerListener::Stub> stub_;

  // Security token for communicating with cicerone.
  std::string token_;

  // Watchers for tracking filesystem changes to .desktop files/dirs.
  std::vector<std::unique_ptr<base::FilePathWatcher>> watchers_;

  // True if there is currently a delayed task pending for updating the
  // application list.
  bool update_app_list_posted_;

  // Closure for stopping the MessageLoop.  Posted to the thread's TaskRunner
  // when this program receives a SIGTERM.
  base::Closure shutdown_closure_;

  // Pointer to the gRPC server so we can shut down its thread when we receive
  // a SIGTERM.
  std::shared_ptr<grpc::Server> grpc_server_;

  // File descriptor for receiving signals.
  base::ScopedFD signal_fd_;
  base::MessageLoopForIO::FileDescriptorWatcher signal_controller_;

  base::WeakPtrFactory<HostNotifier> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(HostNotifier);
};

}  // namespace garcon
}  // namespace vm_tools

#endif  // VM_TOOLS_GARCON_HOST_NOTIFIER_H_
