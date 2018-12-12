// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_CICERONE_TREMPLIN_LISTENER_IMPL_H_
#define VM_TOOLS_CICERONE_TREMPLIN_LISTENER_IMPL_H_

#include <stdint.h>

#include <base/macros.h>
#include <base/memory/weak_ptr.h>
#include <base/sequenced_task_runner.h>
#include <base/time/time.h>
#include <grpc++/grpc++.h>
#include <vm_applications/proto_bindings/apps.pb.h>

#include "tremplin.grpc.pb.h"  // NOLINT(build/include)

namespace vm_tools {
namespace cicerone {

class Service;

// gRPC server implementation for receiving messages from a container in a VM.
class TremplinListenerImpl final
    : public vm_tools::tremplin::TremplinListener::Service {
 public:
  explicit TremplinListenerImpl(
      base::WeakPtr<vm_tools::cicerone::Service> service);
  ~TremplinListenerImpl() override = default;

  grpc::Status TremplinReady(
      grpc::ServerContext* ctx,
      const vm_tools::tremplin::TremplinStartupInfo* request,
      vm_tools::tremplin::EmptyMessage* response) override;

  grpc::Status UpdateCreateStatus(
      grpc::ServerContext* ctx,
      const vm_tools::tremplin::ContainerCreationProgress* request,
      vm_tools::tremplin::EmptyMessage* response) override;

  grpc::Status UpdateStartStatus(
      grpc::ServerContext* ctx,
      const vm_tools::tremplin::ContainerStartProgress* request,
      vm_tools::tremplin::EmptyMessage* response) override;

 private:
  base::WeakPtr<vm_tools::cicerone::Service> service_;  // not owned
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(TremplinListenerImpl);
};

}  // namespace cicerone
}  // namespace vm_tools

#endif  // VM_TOOLS_CICERONE_TREMPLIN_LISTENER_IMPL_H_
