// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_CICERONE_CONTAINER_LISTENER_IMPL_H_
#define VM_TOOLS_CICERONE_CONTAINER_LISTENER_IMPL_H_

#include <stdint.h>

#include <base/macros.h>
#include <base/memory/weak_ptr.h>
#include <base/sequenced_task_runner.h>
#include <base/time/time.h>
#include <grpc++/grpc++.h>

#include "container_host.grpc.pb.h"  // NOLINT(build/include)

namespace vm_tools {
namespace cicerone {

class Service;

// gRPC server implementation for receiving messages from a container in a VM.
class ContainerListenerImpl final
    : public vm_tools::container::ContainerListener::Service {
 public:
  explicit ContainerListenerImpl(
      base::WeakPtr<vm_tools::cicerone::Service> service);
  ~ContainerListenerImpl() override = default;

  // ContainerListener overrides.
  grpc::Status ContainerReady(
      grpc::ServerContext* ctx,
      const vm_tools::container::ContainerStartupInfo* request,
      vm_tools::EmptyMessage* response) override;
  grpc::Status ContainerShutdown(
      grpc::ServerContext* ctx,
      const vm_tools::container::ContainerShutdownInfo* request,
      vm_tools::EmptyMessage* response) override;
  grpc::Status UpdateApplicationList(
      grpc::ServerContext* ctx,
      const vm_tools::container::UpdateApplicationListRequest* request,
      vm_tools::EmptyMessage* response) override;
  grpc::Status OpenUrl(grpc::ServerContext* ctx,
                       const vm_tools::container::OpenUrlRequest* request,
                       vm_tools::EmptyMessage* response) override;
  grpc::Status InstallLinuxPackageProgress(
      grpc::ServerContext* ctx,
      const vm_tools::container::InstallLinuxPackageProgressInfo* request,
      vm_tools::EmptyMessage* response) override;
  grpc::Status UninstallPackageProgress(
      grpc::ServerContext* ctx,
      const vm_tools::container::UninstallPackageProgressInfo* request,
      vm_tools::EmptyMessage* response) override;
  grpc::Status OpenTerminal(
      grpc::ServerContext* ctx,
      const vm_tools::container::OpenTerminalRequest* request,
      vm_tools::EmptyMessage* response) override;
  grpc::Status UpdateMimeTypes(
      grpc::ServerContext* ctx,
      const vm_tools::container::UpdateMimeTypesRequest* request,
      vm_tools::EmptyMessage* response) override;

 private:
  // Returns true if the performing an open window/tab operation will be within
  // the rules for rate limiting, false if it should be blocked. This will also
  // increment the rate limit counter as a side effect.
  bool CheckOpenRateLimit();

  base::WeakPtr<vm_tools::cicerone::Service> service_;  // not owned
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // We rate limit the requests to open a window/tab in Chrome to prevent an
  // accidental DOS of Chrome from a bad script in Linux. We use a fixed window
  // rate control algorithm to do this.
  uint32_t open_count_;
  base::TimeTicks open_rate_window_start_;

  DISALLOW_COPY_AND_ASSIGN(ContainerListenerImpl);
};

}  // namespace cicerone
}  // namespace vm_tools

#endif  // VM_TOOLS_CICERONE_CONTAINER_LISTENER_IMPL_H_
