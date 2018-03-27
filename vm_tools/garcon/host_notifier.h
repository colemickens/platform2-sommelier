// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_GARCON_HOST_NOTIFIER_H_
#define VM_TOOLS_GARCON_HOST_NOTIFIER_H_

#include <memory>
#include <string>
#include <vector>

#include <base/files/file_path_watcher.h>
#include <base/macros.h>
#include <base/memory/weak_ptr.h>
#include <grpc++/grpc++.h>

#include "container_host.grpc.pb.h"  // NOLINT(build/include)

namespace vm_tools {
namespace garcon {

// Handles making calls to concierge running in the host.
class HostNotifier {
 public:
  // Creates and inits the HostNotifier for running on the current sequence.
  // Returns null if there was any failure.
  static std::unique_ptr<HostNotifier> Create();

  ~HostNotifier() = default;

  // Sends a message to the host indicating the container is shutting down.
  static bool NotifyHostOfContainerShutdown();

 private:
  HostNotifier();
  // This will notify the host that garcon is ready and send the initial update
  // for the application list and also establish a watcher for any updates to
  // the list of installed applications. Returns false if there was any failure.
  bool Init();

  // Sends a message to the host indicating that our server is ready for
  // accepting incoming calls.
  bool NotifyHostGarconIsReady();

  // Sends a list of the installed applications to the host.
  bool SendAppListToHost();

  // Sends a list of the installed applications to the host, ignoring return
  // value.
  void SendAppListToHostNoStatus();

  // Callback for when desktop file path changes occur.
  void DesktopPathsChanged(const base::FilePath& path, bool error);

  // gRPC stub for communicating with concierge on the host.
  std::unique_ptr<vm_tools::container::ContainerListener::Stub> stub_;

  // Security token for communicating with concierge.
  std::string token_;

  // Watchers for tracking filesystem changes to .desktop files/dirs.
  std::vector<std::unique_ptr<base::FilePathWatcher>> watchers_;

  // True if there is currently a delayed task pending for updating the
  // application list.
  bool update_app_list_posted_;

  base::WeakPtrFactory<HostNotifier> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(HostNotifier);
};

}  // namespace garcon
}  // namespace vm_tools

#endif  // VM_TOOLS_GARCON_HOST_NOTIFIER_H_
