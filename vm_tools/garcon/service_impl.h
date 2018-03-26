// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_GARCON_SERVICE_IMPL_H_
#define VM_TOOLS_GARCON_SERVICE_IMPL_H_

#include <map>
#include <string>
#include <vector>

#include <base/macros.h>
#include <grpc++/grpc++.h>

#include "container_guest.grpc.pb.h"  // NOLINT(build/include)

namespace vm_tools {
namespace garcon {

// Actually implements the garcon service.
class ServiceImpl final : public vm_tools::container::Garcon::Service {
 public:
  ServiceImpl() = default;
  ~ServiceImpl() override = default;

  // Garcon::Service overrides.
  grpc::Status LaunchApplication(
      grpc::ServerContext* ctx,
      const vm_tools::container::LaunchApplicationRequest* request,
      vm_tools::container::LaunchApplicationResponse* response) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceImpl);
};

}  // namespace garcon
}  // namespace vm_tools

#endif  // VM_TOOLS_GARCON_SERVICE_IMPL_H_
